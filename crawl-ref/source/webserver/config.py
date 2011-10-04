import logging
from collections import OrderedDict

dgl_mode = False

bind_nonsecure = True
bind_address = ""
bind_port = 8080

logging_config = {
    "filename": "webtiles.log",
    "level": logging.INFO,
    "format": "%(asctime)s %(levelname)s: %(message)s"
}

password_db = "./webserver/passwd.db3"

static_path = "./webserver/static"
template_path = "./webserver/templates/"

games = OrderedDict([
    ("dcss-web-trunk", dict(
        name = "DCSS trunk",
        crawl_binary = "./crawl",
        rcfile_path = "./rcs/",
        macro_path = "./rcs/",
        morgue_path = "./rcs",
        running_game_path = "./rcs/running",
        client_prefix = "game")),
    ("sprint-web-trunk", dict(
        name = "Sprint trunk",
        crawl_binary = "./crawl",
        rcfile_path = "./rcs/",
        macro_path = "./rcs/",
        morgue_path = "./rcs",
        running_game_path = "./rcs/running",
        options = ["-sprint"],
        client_prefix = "game")),
    ("zd-web-trunk", dict(
        name = "Zot Defense trunk",
        crawl_binary = "./crawl",
        rcfile_path = "./rcs/",
        macro_path = "./rcs/",
        morgue_path = "./rcs",
        running_game_path = "./rcs/running",
        options = ["-zotdef"],
        client_prefix = "game")),
    ("tut-web-trunk", dict(
        name = "Tutorial trunk",
        crawl_binary = "./crawl",
        rcfile_path = "./rcs/",
        macro_path = "./rcs/",
        morgue_path = "./rcs",
        running_game_path = "./rcs/running",
        options = ["-tutorial"],
        client_prefix = "game")),
])

dgl_status_file = "./rcs/status"

status_file_update_rate = 5

max_connections = 100

init_player_program = "/bin/echo"

# ssl_options = None # No SSL
ssl_options = {
    "certfile": "./webserver/localhost.crt",
    "keyfile": "./webserver/localhost.key"
}
ssl_address = ""
ssl_port = 8081

connection_timeout = 600
max_idle_time = 5 * 60 * 60
http_connection_timeout = 30

kill_timeout = 10 # Seconds until crawl is killed after HUP is sent

nick_regex = r"^[a-zA-Z0-9]{3,20}$"
max_passwd_length = 20

login_token_lifetime = 7 # Days

uid = None  # If this is not None, the server will setuid to that id after
gid = None  # binding its sockets.
