#!/usr/bin/env python2.7
import os, os.path
import subprocess

import tornado.httpserver
import tornado.websocket
import tornado.ioloop
import tornado.web
import tornado.escape
import tornado.template

import crypt
import sqlite3

import logging
import sys
import signal
import time, datetime
import collections
import re
import random
import codecs

from config import *

logging.basicConfig(**logging_config)

class TornadoFilter(logging.Filter):
    def filter(self, record):
        if record.module == "web" and record.levelno <= logging.INFO: return False
        return True
logging.getLogger().addFilter(TornadoFilter())
logging.addLevelName(logging.DEBUG, "DEBG")
logging.addLevelName(logging.WARNING, "WARN")

def user_passwd_match(username, passwd): # Returns the correctly cased username.
    try:
        passwd = passwd[0:max_passwd_length]
        crypted_pw = crypt.crypt(passwd, passwd)
    except:
        return None

    try:
        conn = sqlite3.connect(password_db)
        c = conn.cursor()
        c.execute("select username,password from dglusers where username=? collate nocase",
                  (username,))
        result = c.fetchone()

        if result is None or crypted_pw != result[1]:
            return None
        else:
            return result[0]
    finally:
        if c: c.close()
        if conn: conn.close()

def register_user(username, passwd, email): # Returns an error message or None
    if passwd == "": return "The password can't be empty!"
    passwd = passwd[0:max_passwd_length]
    username = username.strip()
    if not re.match(nick_regex, username): return "Invalid username!"

    crypted_pw = crypt.crypt(passwd, passwd)

    try:
        conn = sqlite3.connect(password_db)
        c = conn.cursor()
        c.execute("select username from dglusers where username=? collate nocase",
                  (username,))
        result = c.fetchone()

        if result: return "User already exists!"

        c.execute("insert into dglusers(username, email, password, flags, env) values (?,?,?,0,'')",
                  (username, email, crypted_pw))

        conn.commit()

        return None
    finally:
        if c: c.close()
        if conn: conn.close()

class DynamicTemplateLoader(tornado.template.Loader):
    def __init__(self, root_dir):
        tornado.template.Loader.__init__(self, root_dir)

    def load(self, name, parent_path=None):
        name = self.resolve_path(name, parent_path=parent_path)
        if name in self.templates:
            template = self.templates[name]
            path = os.path.join(self.root, name)
            stat = os.stat(path)
            if stat.st_mtime > template.load_time:
                del self.templates[name]
            else:
                return template

        template = super(DynamicTemplateLoader, self).load(name, parent_path)
        template.load_time = time.time()
        return template

sockets = set()
current_id = 0
shutting_down = False
login_tokens = {}
rand = random.SystemRandom()

class MainHandler(tornado.web.RequestHandler):
    def get(self):
        host = self.request.host
        if self.request.protocol == "https":
            protocol = "wss://"
        else:
            protocol = "ws://"
        self.render("client.html", socket_server = protocol + host + "/socket",
                    username = None)

class CrawlWebSocket(tornado.websocket.WebSocketHandler):
    def __init__(self, app, req, **kwargs):
        tornado.websocket.WebSocketHandler.__init__(self, app, req, **kwargs)
        self.username = None
        self.p = None
        self.timeout = None
        self.kill_timeout = None
        self.last_action_time = time.time()
        self.watched_game = None
        self.watchers = set()
        self.ttyrec_filename = None
        self.where = None
        self.wheretime = time.time()

        self.ioloop = tornado.ioloop.IOLoop.instance()

        global current_id
        self.id = current_id
        current_id += 1

    def __hash__(self):
        return self.id

    def __eq__(self, other):
        return self.id == other.id

    def open(self):
        logging.info("#%s: Socket opened from ip %s (fd%s).", self.id,
                     self.request.remote_ip, self.request.connection.stream.socket.fileno())
        global sockets
        sockets.add(self)

        self.reset_timeout()

        if max_connections < len(sockets):
            self.write_message("connection_closed('The maximum number of connections "
                               + "has been reached, sorry :(');");
            self.close()
        elif shutting_down:
            self.close()
        else:
            if dgl_mode:
                self.update_lobby()
            else:
                self.start_crawl(None)

    def idle_time(self):
        return time.time() - self.last_action_time

    def is_running(self):
        return self.p is not None

    def is_in_lobby(self):
        return not self.is_running() and self.watched_game is None

    def update_lobby(self):
        if self.client_terminated:
            logging.warn("#%s: update_lobby called for closed " +
                         "socket! (Crawl is %srunning)" %
                         (self.id,
                          ("not " if self.is_running() else "")))
            if not self.is_running():
                global sockets
                sockets.remove(self)
            return

        running_games = [game for game in sockets if game.is_running()]
        lobby_html = self.render_string("lobby.html", running_games = running_games)
        self.write_message("lobby_data(" +
                           tornado.escape.json_encode(lobby_html) + ");")

    def send_game_links(self):
        # Rerender Banner
        banner_html = self.render_string("banner.html", username = self.username)
        self.write_message("$('#banner').html(" +
                           tornado.escape.json_encode(banner_html) + ");");
        play_html = self.render_string("game_links.html", games = games)
        self.write_message("$('#play_now').html(" +
                           tornado.escape.json_encode(play_html) + ");")

    def reset_timeout(self):
        if self.timeout:
            self.ioloop.remove_timeout(self.timeout)

        self.received_pong = False
        self.write_message("ping();")
        self.timeout = self.ioloop.add_timeout(time.time() + connection_timeout,
                                               self.check_connection)

    def check_connection(self):
        self.timeout = None

        if not self.received_pong:
            logging.info("#%s: Connection to remote ip %s timed out.",
                         self.id, self.request.remote_ip)
            self.close()
        else:
            if self.is_running() and self.idle_time() > max_idle_time:
                logging.info("#%s: Stopping crawl after idle time limit for %s.",
                             self.id, self.username)
                self.stop_crawl()

        if not self.client_terminated:
            self.reset_timeout()

    def start_crawl(self, game_id):
        if dgl_mode:
            if self.username == None:
                self.write_message("go_lobby();")
                return

            if game_id not in games: return

        self.last_action_time = time.time()

        self.game_id = game_id

        if dgl_mode:
            game = games[game_id]

            call = [game["crawl_binary"],
                    "-name", self.username,
                    "-rc", os.path.join(game["rcfile_path"], self.username + ".rc"),
                    "-macro", os.path.join(game["macro_path"], self.username + ".macro"),
                    "-morgue", os.path.join(game["morgue_path"], self.username)]

            if "options" in game:
                call += game["options"]

            if "client_prefix" in game:
                self.client_prefix = game["client_prefix"]
                self.send_client(self.client_prefix)
        else:
            call = ["./crawl"]
            self.send_client("game")

        self.p = subprocess.Popen(call,
                                  stdin = subprocess.PIPE,
                                  stdout = subprocess.PIPE,
                                  stderr = subprocess.PIPE)

        self.ioloop.add_handler(self.p.stdout.fileno(), self.on_stdout,
                                self.ioloop.READ | self.ioloop.ERROR)
        self.ioloop.add_handler(self.p.stderr.fileno(), self.on_stderr,
                                self.ioloop.READ | self.ioloop.ERROR)

        logging.info("#%s: Starting crawl for user %s (ip %s, fd%s, fd%s, fd%s).",
                     self.id, self.username, self.request.remote_ip,
                     self.p.stdin.fileno(), self.p.stdout.fileno(),
                     self.p.stderr.fileno())

        self.write_message("crawl_started();")

        if dgl_mode:
            self.create_mock_ttyrec()

            update_global_status()

    def stop_crawl(self):
        self.p.send_signal(subprocess.signal.SIGHUP)
        self.kill_timeout = self.ioloop.add_timeout(time.time() + kill_timeout, self.kill_crawl)

    def kill_crawl(self):
        if self.p:
            logging.info("#%s: Killing crawl process after SIGHUP did nothing (user %s, ip %s).",
                         self.id, self.username, self.request.remote_ip)
            self.p.send_signal(subprocess.signal.SIGTERM)
            self.kill_timeout = None

    def init_user(self):
        with open("/dev/null", "w") as f:
            result = subprocess.call([init_player_program, self.username],
                                     stdout = f, stderr = subprocess.STDOUT)
            return result == 0

    def create_mock_ttyrec(self):
        now = datetime.datetime.utcnow()
        running_game_path = games[self.game_id]["running_game_path"]
        self.ttyrec_filename = os.path.join(running_game_path,
                                            self.username + ":" + now.strftime("%Y-%m-%d.%H:%M:%S")
                                            + ".ttyrec")
        f = open(self.ttyrec_filename, "w")
        f.close()

    def delete_mock_ttyrec(self):
        if self.ttyrec_filename:
            os.remove(self.ttyrec_filename)
            self.ttyrec_filename = None

    def check_where(self):
        morgue_path = games[self.game_id]["morgue_path"]
        wherefile = os.path.join(morgue_path, self.username, self.username + ".dglwhere")
        try:
            stat = os.stat(wherefile)
            if stat.st_mtime > self.wheretime:
                self.wheretime = time.time()
                f = open(wherefile, "r")
                _, _, newwhere = f.readline().partition("|")
                f.close()

                newwhere = newwhere.strip()

                if self.where != newwhere:
                    self.where = newwhere
                    update_global_status()
        except (OSError, IOError):
            pass

    def add_watcher(self, watcher):
        self.watchers.add(watcher)
        watcher.send_client(self.client_prefix)
        self.p.stdin.write("^r") # Redraw
        self.update_watcher_description()

    def remove_watcher(self, watcher):
        self.watchers.remove(watcher)
        self.update_watcher_description()

    def update_watcher_description(self):
        watcher_names = [watcher.username for watcher in self.watchers
                         if watcher.username]
        anon_count = len(self.watchers) - len(watcher_names)
        s = ", ".join(watcher_names)
        if len(watcher_names) > 0 and anon_count > 0:
            s = s + ", and %i Anon" % anon_count
        elif anon_count > 0:
            s = "%i Anon" % anon_count
        self.write_message_all("watchers(%i,'%s')" %
                               (len(self.watchers), s))

    def stop_watching(self):
        if self.watched_game:
            logging.info("#%s: User %s stopped watching %s #%s (ip: %s)",
                         self.id, self.username, self.watched_game.username,
                         self.watched_game.id, self.request.remote_ip)
            self.watched_game.remove_watcher(self)
            self.watched_game = None
            self.write_message("set_watching(false);")
            self.write_message("go_lobby();")

    def send_client(self, client_prefix):
        game_html = self.render_string(client_prefix + "/game.html", prefix = client_prefix)
        self.write_message("delay_timeout = 1;$('#game').html(" +
                           tornado.escape.json_encode(game_html) + ");delay_ended();")

    def shutdown(self):
        if not self.client_terminated:
            msg = self.render_string("shutdown.html", game=self)
            self.write_message("connection_closed(" +
                               tornado.escape.json_encode(msg) + ");")
            self.close()
        if self.is_running():
            self.stop_crawl()

    def poll_crawl(self):
        if self.p is not None and self.p.poll() is not None:
            self.ioloop.remove_handler(self.p.stdout.fileno())
            self.ioloop.remove_handler(self.p.stderr.fileno())
            self.p.stdout.close()
            self.p.stderr.close()
            self.p = None

            logging.info("#%s: Crawl terminated.", self.id)

            if self.kill_timeout:
                self.ioloop.remove_timeout(self.kill_timeout)
                self.kill_timeout = None

            if dgl_mode: self.delete_mock_ttyrec()
            self.where = None

            if self.client_terminated:
                global sockets
                sockets.remove(self)
            else:
                if shutting_down:
                    self.close()
                else:
                    # Go back to lobby
                    self.write_message("crawl_ended();")
                    if dgl_mode:
                        self.update_lobby()
                    else:
                        self.start_crawl(None)

            if dgl_mode: update_global_status()

            for watcher in list(self.watchers):
                watcher.stop_watching()

            if shutting_down and len(sockets) == 0:
                # The last crawl process has ended, now we can go
                self.ioloop.stop()

    def login(self, username):
        self.username = username
        if not self.init_user():
            self.write_message("connection_closed('Could not initialize your rc and morgue!<br>" +
                               "This probably means there is something wrong with the server " +
                               "configuration.');")
            logging.warn("#%s: User initialization returned an error for user %s!",
                         self.id, self.username)
            self.username = None
            self.close()
            return
        self.write_message("logged_in(" +
                           tornado.escape.json_encode(username) + ");")
        self.send_game_links()

    def on_message(self, message):
        login_start = "Login: "
        if message.startswith(login_start):
            message = message[len(login_start):]
            username, _, password = message.partition(' ')
            real_username = user_passwd_match(username, password)
            if real_username:
                logging.info("#%s: User %s logged in from ip %s.",
                             self.id, real_username, self.request.remote_ip)
                self.login(real_username)
            else:
                logging.warn("#%s: Failed login for user %s from ip %s.",
                             self.id, username, self.request.remote_ip)
                self.write_message("login_failed();")

        elif message.startswith("LoginToken: "):
            message = message[len("LoginToken: "):]
            username, _, token = message.partition(' ')
            token = long(token)
            if (token, username) in login_tokens:
                del login_tokens[(token, username)]
                logging.info("#%s: User %s logged in from ip %s (via token).",
                             self.id, username, self.request.remote_ip)
                self.login(username)
            else:
                logging.warn("#%s: Wrong login token for user %s from ip %s.",
                             self.id, username, self.request.remote_ip)
                self.write_message("login_failed();")

        elif message == "Remember":
            if self.username is None: return
            token = rand.getrandbits(128)
            expires = datetime.datetime.now() + datetime.timedelta(login_token_lifetime)
            login_tokens[(token, self.username)] = expires
            cookie = self.username + " " + str(token)
            self.write_message("set_login_cookie(" +
                               tornado.escape.json_encode(cookie) + "," +
                               str(login_token_lifetime) + ");")

        elif message.startswith("UnRemember: "):
            message = message[len("UnRemember: "):]
            username, _, token = message.partition(' ')
            try:
                token = long(token)
                if (token, username) in login_tokens:
                    del login_tokens[(token, username)]
            except ValueError:
                return

        elif message.startswith("Play: "):
            if self.p or self.watched_game: return
            game_id = message[len("Play: "):]
            self.start_crawl(game_id)

        elif message == "Pong":
            self.received_pong = True

        elif message == "UpdateLobby":
            self.update_lobby()

        elif message.startswith("Watch: "):
            if self.p or self.watched_game: return
            watch_username = message[len("Watch: "):]
            sockets = [socket for socket in find_user_sockets(watch_username)
                       if socket.is_running()]
            if len(sockets) >= 1:
                socket = sockets[0]
                logging.info("#%s: User %s (ip: %s) started watching #%s %s.",
                             self.id, self.username, self.request.remote_ip,
                             socket.id, socket.username)
                self.watched_game = socket
                socket.add_watcher(self)
                self.write_message("set_watching(true);")
            else:
                self.write_message("go_lobby();")

        elif message.startswith("Chat: "):
            if self.username is None:
                self.write_message("chat('You need to log in to send messages!');")
                return

            chat_msg = ("<span class='chat_sender'>%s</span>: <span class='chat_msg'>%s</span>" %
                        (self.username, tornado.escape.xhtml_escape(message[len("Chat: "):])))
            receiver = None
            if self.p:
                receiver = self
            elif self.watched_game:
                receiver = self.watched_game
            if receiver:
                receiver.write_message_all("chat(%s);" % tornado.escape.json_encode(chat_msg))

        elif message.startswith("Register: "):
            message = message[len("Register: "):]
            username, _, message = message.partition(" ")
            email, _, password = message.partition(" ")
            error = register_user(username, password, email)
            if error is None:
                logging.info("#%s: Registered user: %s (ip: %s)", self.id, username,
                             self.request.remote_ip)
                self.login(username)
            else:
                logging.info("#%s: Registration attempt failed for username %s: %s (ip: %s)",
                             self.id, username, error, self.request.remote_ip)
                self.write_message("register_failed(" +
                                   tornado.escape.json_encode(error) + ");")

        elif message == "GoLobby":
            if self.is_running():
                self.stop_crawl()
            elif self.watched_game:
                self.stop_watching()

        elif message.startswith("GetRC: "):
            game_id = message[len("GetRC: "):]
            if game_id not in games: return
            rcfile_path = os.path.join(games[game_id]["rcfile_path"], self.username + ".rc")
            with open(rcfile_path, 'r') as f:
                contents = f.read()
            self.write_message("rcfile_contents(" +
                               tornado.escape.json_encode(contents) + ");")

        elif message.startswith("SetRC: "):
            message = message[len("SetRC: "):]
            game_id, _, contents = message.partition(" ")
            rcfile_path = os.path.join(games[game_id]["rcfile_path"], self.username + ".rc")
            with open(rcfile_path, 'w') as f:
                f.write(codecs.BOM_UTF8)
                f.write(contents.encode("utf8"))

        elif self.p is not None:
            if not message.startswith("^"):
                self.last_action_time = time.time()

            logging.debug("#%s: Message: %s (user: %s)", self.id, message, self.username)
            self.poll_crawl()
            if self.p is not None:
                self.p.stdin.write(message.encode("utf8"))

    def write_message(self, msg):
        try:
            super(CrawlWebSocket, self).write_message(msg)
        except:
            logging.warn("#%s: Exception trying to send message to %s.",
                         self.id, self.request.remote_ip, exc_info = True)
            self.ws_connection._abort()

    def write_message_all(self, msg):
        if not self.client_terminated:
            self.write_message(msg)

        for watcher in self.watchers:
            if not watcher.client_terminated:
                watcher.write_message(msg)

    def on_close(self):
        global sockets
        if self.p is None and self in sockets:
            sockets.remove(self)
            if shutting_down and len(sockets) == 0:
                # The last socket has been closed, now we can go
                self.ioloop.stop()
        elif self.is_running():
            self.stop_crawl()

        if self.watched_game:
            self.watched_game.remove_watcher(self)

        if self.timeout:
            self.ioloop.remove_timeout(self.timeout)

        logging.info("#%s: Socket for ip %s closed.", self.id, self.request.remote_ip)

    def on_stderr(self, fd, events):
        if events & self.ioloop.ERROR:
            self.poll_crawl()
        elif events & self.ioloop.READ:
            s = self.p.stderr.readline()

            if self.client_terminated:
                return

            if not (s.isspace() or s == ""):
                logging.info("#%s: ERR: %s: %s",
                             self.id, self.username, s.strip())

            self.poll_crawl()

    def handle_crawl_message(self, msg):
        if msg.startswith("^"):
            msg = msg[1:]
            if msg.startswith("ClientPrefix:"):
                msg = msg[len("ClientPrefix:"):]
                msg = msg.strip()
                self.client_prefix = msg
                logging.info("#%s: User %s is using client version %s.",
                             self.id, self.username, self.client_prefix)
                self.send_client(self.client_prefix)
        else:
            self.write_message_all(msg)

    def on_stdout(self, fd, events):
        if events & self.ioloop.ERROR:
            self.poll_crawl()
        elif events & self.ioloop.READ:
            msg = self.p.stdout.readline()

            if self.client_terminated:
                return

            self.handle_crawl_message(msg)

            self.poll_crawl()
            if dgl_mode: self.check_where()

def update_global_status():
    update_all_lobbys()
    write_dgl_status_file()

def update_all_lobbys():
    for socket in list(sockets):
        if socket.is_in_lobby():
            socket.update_lobby()

def write_dgl_status_file():
    try:
        f = open(dgl_status_file, "w")
        for socket in list(sockets):
            if socket.username and socket.is_running():
                f.write(socket.username + "#" +
                        socket.game_id + "#" +
                        (socket.where or "") + "#" +
                        "0x0" + "#" +
                        str(int(socket.idle_time())) + "#" +
                        str(len(socket.watchers)) + "#\n")
    except (OSError, IOError) as e:
        logging.warn("Could not write dgl status file: %s", e)
    finally:
        if f: f.close()

def status_file_timeout():
    write_dgl_status_file()
    ioloop.add_timeout(time.time() + status_file_update_rate,
                       status_file_timeout)

def find_user_sockets(username):
    for socket in list(sockets):
        if socket.username and socket.username.lower() == username.lower():
            yield socket

def shutdown():
    global shutting_down
    shutting_down = True
    for socket in list(sockets):
        socket.shutdown()

def handler(signum, frame):
    logging.info("Received signal %i, shutting down.", signum)
    shutdown()
    if len(sockets) == 0:
        ioloop.stop()

def purge_login_tokens():
    for token in list(login_tokens):
        if datetime.datetime.now() > login_tokens[token]:
            del login_tokens[token]

def purge_login_tokens_timeout():
    purge_login_tokens()
    ioloop.add_timeout(time.time() + 60 * 60 * 1000,
                       purge_login_tokens_timeout)

signal.signal(signal.SIGTERM, handler)
signal.signal(signal.SIGHUP, handler)

settings = {
    "static_path": static_path,
    "template_loader": DynamicTemplateLoader(template_path)
}

application = tornado.web.Application([
    (r"/", MainHandler),
    (r"/socket", CrawlWebSocket),
], **settings)

kwargs = {}
if http_connection_timeout is not None:
    kwargs["connection_timeout"] = http_connection_timeout

if bind_nonsecure:
    application.listen(bind_port, bind_address, **kwargs)
if ssl_options:
    application.listen(ssl_port, ssl_address, ssl_options = ssl_options,
                       **kwargs)

if gid is not None:
    os.setgid(gid)
if uid is not None:
    os.setuid(uid)

ioloop = tornado.ioloop.IOLoop.instance()
ioloop.set_blocking_log_threshold(0.5)

if dgl_mode:
    status_file_timeout()
    purge_login_tokens_timeout()

logging.info("Webtiles server started!")

try:
    ioloop.start()
except KeyboardInterrupt:
    logging.info("Received keyboard interrupt, shutting down.")
    shutdown()
    if len(sockets) > 0:
        ioloop.start() # We'll wait until all crawl processes have ended.
