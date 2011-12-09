from tornado.escape import json_encode, json_decode, xhtml_escape
import tornado.websocket
import tornado.ioloop
import tornado.template

import os
import subprocess
import logging
import signal
import time, datetime
import codecs
import random

import config
from userdb import *
from util import *

sockets = set()
current_id = 0
shutting_down = False
login_tokens = {}
rand = random.SystemRandom()

def shutdown():
    global shutting_down
    shutting_down = True
    for socket in list(sockets):
        socket.shutdown()

def update_global_status():
    write_dgl_status_file()

def update_all_lobbys(game):
    lobby_entry = game.lobby_entry()
    for socket in list(sockets):
        if socket.is_in_lobby():
            socket.send_message("lobby_entry", **lobby_entry)

def write_dgl_status_file():
    f = None
    try:
        f = open(config.dgl_status_file, "w")
        for socket in list(sockets):
            if socket.username and socket.is_running():
                f.write("%s#%s#%s#0x0#%s#%s#\n" %
                        (socket.username, socket.game_id,
                         (socket.process.human_readable_where()),
                         str(socket.process.idle_time()),
                         str(socket.process.watcher_count())))
    except (OSError, IOError) as e:
        logging.warning("Could not write dgl status file: %s", e)
    finally:
        if f: f.close()

def purge_login_tokens():
    for token in list(login_tokens):
        if datetime.datetime.now() > login_tokens[token]:
            del login_tokens[token]

def purge_login_tokens_timeout():
    purge_login_tokens()
    ioloop = tornado.ioloop.IOLoop.instance()
    ioloop.add_timeout(time.time() + 60 * 60 * 1000,
                       purge_login_tokens_timeout)

def status_file_timeout():
    write_dgl_status_file()
    ioloop = tornado.ioloop.IOLoop.instance()
    ioloop.add_timeout(time.time() + config.status_file_update_rate,
                       status_file_timeout)

def find_user_sockets(username):
    for socket in list(sockets):
        if socket.username and socket.username.lower() == username.lower():
            yield socket

def find_running_game(charname, start):
    for socket in list(sockets):
        if (socket.is_running() and
            socket.process.where.get("name") == charname and
            socket.process.where.get("start") == start):
            return socket.process
    return None

milestone_file_tailer = None
def start_reading_milestones():
    if config.milestone_file is None: return

    global milestone_file_tailer
    milestone_file_tailer = FileTailer(config.milestone_file, handle_new_milestone)

def handle_new_milestone(line):
    data = parse_where_data(line)
    if "name" not in data: return
    game = find_running_game(data.get("name"), data.get("start"))
    game.log_milestone(data)

class CrawlWebSocket(tornado.websocket.WebSocketHandler):
    def __init__(self, app, req, **kwargs):
        tornado.websocket.WebSocketHandler.__init__(self, app, req, **kwargs)
        self.username = None
        self.timeout = None
        self.watched_game = None
        self.process = None
        self.game_id = None
        self.received_pong = None

        self.ioloop = tornado.ioloop.IOLoop.instance()

        global current_id
        self.id = current_id
        current_id += 1

        self.logger = logging.LoggerAdapter(logging.getLogger(), {})
        self.logger.process = self._process_log_msg

        self.message_handlers = {
            "login": self.login,
            "token_login": self.token_login,
            "set_login_cookie": self.set_login_cookie,
            "forget_login_cookie": self.forget_login_cookie,
            "play": self.start_crawl,
            "pong": self.pong,
            "watch": self.watch,
            "chat_msg": self.post_chat_message,
            "register": self.register,
            "go_lobby": self.go_lobby,
            "get_rc": self.get_rc,
            "set_rc": self.set_rc,
            }

    def _process_log_msg(self, msg, kwargs):
        return "#%-5s %s" % (self.id, msg), kwargs

    def __hash__(self):
        return self.id

    def __eq__(self, other):
        return self.id == other.id

    def open(self):
        self.logger.info("Socket opened from ip %s (fd%s).",
                         self.request.remote_ip,
                         self.request.connection.stream.socket.fileno())
        sockets.add(self)

        self.reset_timeout()

        if config.max_connections < len(sockets):
            self.write_message("connection_closed('The maximum number of connections "
                               + "has been reached, sorry :(');")
            self.close()
        elif shutting_down:
            self.close()
        else:
            if config.dgl_mode:
                self.send_lobby()
            else:
                self.start_crawl(None)

    def idle_time(self):
        return self.process.idle_time()

    def is_running(self):
        return self.process is not None

    def is_in_lobby(self):
        return not self.is_running() and self.watched_game is None

    def send_lobby(self):
        self.send_message("lobby_clear")
        for socket in sockets:
            if socket.is_running():
                self.send_message("lobby_entry", **socket.process.lobby_entry())
        self.send_message("lobby_complete")

    def send_game_links(self):
        # Rerender Banner
        banner_html = self.render_string("banner.html", username = self.username)
        self.send_message("html", id = "banner", content = banner_html)
        play_html = self.render_string("game_links.html", games = config.games)
        self.send_message("set_game_links", content = play_html)

    def reset_timeout(self):
        if self.timeout:
            self.ioloop.remove_timeout(self.timeout)

        self.received_pong = False
        self.send_message("ping")
        self.timeout = self.ioloop.add_timeout(time.time() + config.connection_timeout,
                                               self.check_connection)

    def check_connection(self):
        self.timeout = None

        if not self.received_pong:
            self.logger.info("Connection timed out.")
            self.close()
        else:
            if self.is_running() and self.process.idle_time() > config.max_idle_time:
                self.logger.info("Stopping crawl after idle time limit.")
                self.process.stop()

        if not self.client_terminated:
            self.reset_timeout()

    def start_crawl(self, game_id):
        if config.dgl_mode:
            if self.username == None:
                self.send_message("go_lobby")
                return

        if config.dgl_mode and game_id not in config.games: return

        self.game_id = game_id

        import process_handler

        if config.dgl_mode:
            game_params = dict(config.games[game_id])
            game_params["id"] = game_id
            args = (game_params, self.username, self.logger, self.ioloop)
            if (game_params.get("compat_mode") or
                "client_prefix" in game_params):
                self.process = process_handler.CompatCrawlProcessHandler(*args)
            else:
                self.process = process_handler.CrawlProcessHandler(*args)
        else:
            self.process = process_handler.DGLLessCrawlProcessHandler(self.logger, self.ioloop)

        self.process.end_callback = self._on_crawl_end
        self.process.add_watcher(self, hide=True)
        try:
            self.process.start()
        except:
            self.logger.warning("Exception starting process!", exc_info=True)
            self.process = None
            self.send_message("go_lobby")
        else:
            self.send_message("game_started")

            if config.dgl_mode:
                if self.process.where == {}:
                    # If location info was found, the lobbys were already
                    # updated by set_where_data
                    update_all_lobbys(self.process)
                update_global_status()

    def _on_crawl_end(self):
        if config.dgl_mode:
            for socket in list(sockets):
                if socket.is_in_lobby():
                    socket.send_message("lobby_remove", id=self.process.id)
        self.process = None

        if self.client_terminated:
            sockets.remove(self)
        else:
            if shutting_down:
                self.close()
            else:
                # Go back to lobby
                self.send_message("game_ended")
                if config.dgl_mode:
                    self.send_lobby()
                else:
                    self.start_crawl(None)

        if config.dgl_mode:
            update_global_status()

        if shutting_down and len(sockets) == 0:
            # The last crawl process has ended, now we can go
            self.ioloop.stop()

    def init_user(self):
        with open("/dev/null", "w") as f:
            result = subprocess.call([config.init_player_program, self.username],
                                     stdout = f, stderr = subprocess.STDOUT)
            return result == 0

    def stop_watching(self):
        if self.watched_game:
            self.logger.info("Stopped watching %s.", self.watched_game.username)
            self.watched_game.remove_watcher(self)
            self.watched_game = None
            self.send_message("go_lobby")

    def shutdown(self):
        if not self.client_terminated:
            msg = self.render_string("shutdown.html", game=self)
            self.send_message("close", reason = msg)
            self.close()
        if self.is_running():
            self.process.stop()

    def do_login(self, username):
        self.username = username
        self.logger.extra["username"] = username
        if not self.init_user():
            msg = ("Could not initialize your rc and morgue!<br>" +
                   "This probably means there is something wrong " +
                   "with the server configuration.")
            self.send_message("close", reason = msg)
            self.logger.warning("User initialization returned an error for user %s!",
                                self.username)
            self.username = None
            self.close()
            return
        self.send_message("login_success", username = username)
        self.send_game_links()

    def login(self, username, password):
        real_username = user_passwd_match(username, password)
        if real_username:
            self.logger.info("User %s logged in.", real_username)
            self.do_login(real_username)
        else:
            self.logger.warning("Failed login for user %s.", username)
            self.send_message("login_fail")

    def token_login(self, cookie):
        username, _, token = cookie.partition(' ')
        token = long(token)
        if (token, username) in login_tokens:
            del login_tokens[(token, username)]
            self.logger.info("User %s logged in (via token).", username)
            self.do_login(username)
        else:
            self.logger.warning("Wrong login token for user %s.", username)
            self.send_message("login_fail")

    def set_login_cookie(self):
        if self.username is None: return
        token = rand.getrandbits(128)
        expires = datetime.datetime.now() + datetime.timedelta(config.login_token_lifetime)
        login_tokens[(token, self.username)] = expires
        cookie = self.username + " " + str(token)
        self.send_message("login_cookie", cookie = cookie,
                          expires = config.login_token_lifetime)

    def forget_login_cookie(self, cookie):
        try:
            username, _, token = cookie.partition(' ')
            token = long(token)
            if (token, username) in login_tokens:
                del login_tokens[(token, username)]
        except ValueError:
            return

    def pong(self):
        self.received_pong = True

    def watch(self, username):
        procs = [socket.process for socket in find_user_sockets(username)
                 if socket.is_running()]
        if len(procs) >= 1:
            process = procs[0]
            self.logger.info("Started watching %s.", process.username)
            self.watched_game = process
            process.add_watcher(self)
            self.send_message("watching_started")
        else:
            self.send_message("go_lobby")

    def post_chat_message(self, text):
        if self.username is None:
            self.send_message("chat",
                              content = 'You need to log in to send messages!')
            return

        chat_msg = ("<span class='chat_sender'>%s</span>: <span class='chat_msg'>%s</span>" %
                    (self.username, xhtml_escape(text)))
        receiver = None
        if self.process:
            receiver = self.process
        elif self.watched_game:
            receiver = self.watched_game

        if receiver:
            receiver.send_to_all("chat", content = chat_msg)

    def register(self, username, password, email):
        error = register_user(username, password, email)
        if error is None:
            self.logger.info("Registered user %s.", username)
            self.do_login(username)
        else:
            self.logger.info("Registration attempt failed for username %s: %s",
                             username, error)
            self.send_message("register_fail", reason = error)

    def go_lobby(self):
        if self.is_running():
            self.process.stop()
        elif self.watched_game:
            self.stop_watching()

    def get_rc(self, game_id):
        if game_id not in config.games: return
        rcfile_path = dgl_format_str(config.games[game_id]["rcfile_path"],
                                     self.username, config.games[game_id])
        rcfile_path = os.path.join(rcfile_path, self.username + ".rc")
        with open(rcfile_path, 'r') as f:
            contents = f.read()
        self.send_message("rcfile_contents", contents = contents)

    def set_rc(self, game_id, contents):
        rcfile_path = dgl_format_str(config.games[game_id]["rcfile_path"],
                                     self.username, config.games[game_id])
        rcfile_path = os.path.join(rcfile_path, self.username + ".rc")
        with open(rcfile_path, 'w') as f:
            f.write(codecs.BOM_UTF8)
            f.write(contents.encode("utf8"))

    def on_message(self, message):
        if message.startswith("{"):
            try:
                obj = json_decode(message)
                if obj["msg"] in self.message_handlers:
                    handler = self.message_handlers[obj["msg"]]
                    del obj["msg"]
                    handler(**obj)
                elif self.process:
                    self.process.handle_input(message)
                else:
                    self.logger.warning("Didn't know how to handle msg: %s",
                                        obj["msg"])
            except:
                self.logger.warning("Error while handling JSON message!",
                                    exc_info=True)

        elif self.process:
            # This is just for compatibility with 0.9, 0.10 only sends
            # JSON
            self.process.handle_input(message)

    def write_message(self, msg):
        try:
            if not self.client_terminated:
                super(CrawlWebSocket, self).write_message(msg)
        except:
            self.logger.warning("Exception trying to send message.", exc_info = True)
            self.ws_connection._abort()

    def send_message(self, msg, **data):
        """Sends a JSON message to the client."""
        data["msg"] = msg
        if not self.client_terminated:
            self.write_message(json_encode(data))

    def on_close(self):
        if self.process is None and self in sockets:
            sockets.remove(self)
            if shutting_down and len(sockets) == 0:
                # The last socket has been closed, now we can go
                self.ioloop.stop()
        elif self.is_running():
            self.process.stop()

        if self.watched_game:
            self.watched_game.remove_watcher(self)

        if self.timeout:
            self.ioloop.remove_timeout(self.timeout)

        self.logger.info("Socket closed.")
