#include <yed/plugin.h>
#include <sys/stat.h>

static int                cmd_buff_is_running;
static int                read_cmd_buff;
static yed_nb_subproc_t   nb_subproc;
static yed_event_handler  pump_handler;
static yed_event_handler  ypm_list_handler;
array_t                   plugin_arr;
char                     *plugin_tmp_name;
void(*fn)(void);
yed_plugin *ypm_self;

typedef struct plugin_t {
    int     downloaded;
    int     compiled;
    int     installed;
    int     loaded;
    char   *version;
    int     error;
    char   *error_str;
    char   *plugin_name;
} plugin;

/* Must Set plugins-add-dir '~/.yed/ypm_plugins' in yedrc */

static yed_buffer *get_or_make_buffer(char *buff_name);
static void yed_run_command_in_background(char *cmd);
static void yed_grab_plugin_names(void);
void cmd_update_running(void);
void cmd_pump_handler(yed_event *event);
void list_pump_handler(yed_event *event);
int set_template_completion(char *name, struct yed_completion_results_t *comp_res);
void yed_open_buffer_window(char *buff);
void start_event_handler();

static void ypm_setup(void);
static void ypm_init(int n_args, char **args);
static void ypm_list(int n_args, char **args);
static void ypm_help(int n_args, char **args);
static void ypm_install(int n_args, char **args);
static void ypm_uninstall(int n_args, char **args);
static void ypm_update(int n_args, char **args);

void start_event_handler() {
    cmd_buff_is_running = 0;
    read_cmd_buff = 0;

    pump_handler.kind = EVENT_PRE_PUMP;
    pump_handler.fn   = cmd_pump_handler;

    yed_plugin_add_event_handler(ypm_self, pump_handler);
}

static yed_buffer *get_or_make_buffer(char *buff_name) {
    yed_buffer *buff;
    char buff_name_final[512];

    buff_name_final[0] = 0;
    strcat(buff_name_final, "*");
    strcat(buff_name_final, buff_name);

    buff = yed_get_buffer(buff_name_final);
    if (buff == NULL) {
        buff = yed_create_buffer(buff_name_final);
        buff->flags |= BUFF_SPECIAL | BUFF_RD_ONLY;
    }

    return buff;
}

void cmd_update_running(void) {
    yed_frame **fit;
    int         last_row;
    yed_buffer *cmd_buff;

    cmd_buff = get_or_make_buffer("ypm-cmd-buff");
    cmd_buff->flags &= ~BUFF_RD_ONLY;
    cmd_buff_is_running = yed_read_subproc_into_buffer_nb(&nb_subproc);
    cmd_buff->flags |= BUFF_RD_ONLY;

    if (!cmd_buff_is_running && nb_subproc.err == 0) {
        read_cmd_buff = 1;
    }

    array_traverse(ys->frames, fit) {
        if (*fit == ys->active_frame) { continue; }
        if ((*fit)->buffer == cmd_buff) {
            last_row = yed_buff_n_lines((*fit)->buffer);
            yed_set_cursor_far_within_frame(*fit, last_row, 1);
        }
    }
}

void list_pump_handler(yed_event *event) {
    yed_buffer *buff;
    yed_line   *line;
    char       *bname;

    buff = get_or_make_buffer("ypm_buff");

    if ((event->key != ENTER)
    ||  ys->interactive_command
    ||  !ys->active_frame
    ||  ys->active_frame->buffer != buff) {
        return;
    }

    if(event->key == ENTER && (event->row >= 5)) {
        line  = yed_buff_get_line(buff, ys->active_frame->cursor_line);
        array_zero_term(line->chars);
        bname = array_data(line->chars);


    }

}

void cmd_pump_handler(yed_event *event) {
    yed_buffer *cmd_buff;
    char       *buffer;

    cmd_buff = get_or_make_buffer("ypm-cmd-buff");
    cmd_buff->flags &= ~BUFF_RD_ONLY;

    if (cmd_buff_is_running) {
        cmd_update_running();
    }else if(cmd_buff != NULL && read_cmd_buff){
        if(fn != NULL) {
            yed_buff_insert_string(cmd_buff, "Running fn", yed_buff_n_lines(cmd_buff)+1, 1);
            fn();
            fn = NULL;
        }
        yed_delete_event_handler(pump_handler);
        yed_buff_insert_string(cmd_buff, "Done!", yed_buff_n_lines(cmd_buff)+1, 1);
        cmd_buff->flags |= BUFF_RD_ONLY;
    }
}

static void yed_run_command_in_background(char *cmd) {
        yed_buffer *cmd_buff;

        cmd_buff = get_or_make_buffer("ypm-cmd-buff");
        cmd_buff->flags &= ~BUFF_RD_ONLY;

        if(yed_start_read_subproc_into_buffer_nb(cmd, cmd_buff, &nb_subproc)) {
            cmd_buff->flags |= BUFF_RD_ONLY;
            cmd_buff_is_running = 0;
            yed_cerr("there was an error when calling yed_start_read_subproc_into_buffer_nb() in ypm_init()");
            return;
        }else{
            cmd_buff_is_running = 1;
        }
        cmd_buff->flags |= BUFF_RD_ONLY;

        usleep(15000);
        cmd_update_running();
}

static void yed_grab_plugin_names(void) {
    char *plugins_path;
    char *plugin_name;
    char plugin_sub_path[512];
    char loc[256];
    char loc2[256];
    char *so_path;
    struct stat buffer;

    char version_path[512];
    FILE *fp;
    char line[512];

    DIR *d1;
    struct dirent *dir1;
    array_t tmp_arr;
    tmp_arr = array_make(char *);
    char *tmp_so_name;
    char *ptr;
    const char ch = '.';

    so_path = abs_path("~/.yed/ypm/plugins", loc2);
    d1 = opendir(so_path);
    if(d1) {
        while((dir1 = readdir(d1)) != NULL ) {
            tmp_so_name = strdup(dir1->d_name);
            if(strlen(tmp_so_name) >= 4) {
                tmp_so_name[strlen(tmp_so_name)-3] = 0;
            }
            array_push(tmp_arr, tmp_so_name);
        }
    }
    closedir(d1);

    if(array_len(plugin_arr) != 0) {
        array_free(plugin_arr);
    }
    plugin_arr = array_make(plugin);
    plugins_path = abs_path("~/.yed/ypm/yed_plugins", loc);
    if(plugins_path == NULL) {
        yed_cerr("Must run ypm-init command");
        return;
    }

    DIR *d;
    struct dirent *dir;
    d = opendir(plugins_path);
    if(d) {
        while((dir = readdir(d)) != NULL) {
            plugin_name = strdup(dir->d_name);
            if((strcmp(plugin_name, ".")) && (strcmp(plugin_name, "..")) &&                 \
                   (strcmp(plugin_name, ".git")) && (strcmp(plugin_name, ".gitmodules")) && \
                   (strcmp(plugin_name, "README.md"))                               \
              ) {
                plugin tmp_plugin;
                //name
                tmp_plugin.plugin_name = strdup(plugin_name);

                //downloaded
                plugin_sub_path[0] = 0;
                sprintf(plugin_sub_path ,"%s/%s/build.sh", plugins_path, plugin_name);
                if(stat(plugin_sub_path, &buffer) == 0) {
                    tmp_plugin.downloaded = 1;
                }else{
                    tmp_plugin.downloaded = 0;
                }

                //compiled
                plugin_sub_path[0] = 0;
                sprintf(plugin_sub_path ,"%s/%s/%s.so", plugins_path, plugin_name, plugin_name);
/*                 yed_cerr("Y %s", plugin_sub_path); */
                if(stat(plugin_sub_path, &buffer) == 0) {
                    tmp_plugin.compiled = 1;
                }else{
/*                 yed_cerr("N %s", plugin_sub_path); */
                    tmp_plugin.compiled = 0;
                }

                //installed
                char **it;
                tmp_plugin.installed = 0;
                array_traverse(tmp_arr, it) {
                    if(strcmp(*it, plugin_name) == 0) {
                        tmp_plugin.installed = 1;
                        break;
                    }
                }

                //loaded
                tmp_plugin.loaded = 0;
                tree_it(yed_plugin_name_t, yed_plugin_ptr_t) tree_it;
                tree_it = tree_lookup(ys->plugins, plugin_name);
                if (tree_it_good(tree_it)) {
                    tmp_plugin.loaded = 1;
                }


                //version
                tmp_plugin.version = "";
                if(tmp_plugin.loaded) {
                    so_path = abs_path("~/.yed/ypm/yed_plugins", loc2);
                    version_path[0] = 0;
                    strcat(version_path, so_path);
                    strcat(version_path, "/");
                    strcat(version_path, plugin_name);
                    strcat(version_path, "/version.txt");
                    fp = fopen(version_path, "r");
                    if(fp != NULL) {
                        fgets(line, 512, fp );
                        if(line[strlen(line)-1] == '\n') {
                            line[strlen(line)-1] = 0;
                        }
                        tmp_plugin.version = strdup(line);
                    }
                }

                //error
                tmp_plugin.error = 0;

                array_push(plugin_arr, tmp_plugin);
            }
        }
        closedir(d);
    }
}

int set_template_completion(char *name, struct yed_completion_results_t *comp_res) {
    int ret = 0;
    array_t tmp;
    plugin *it;

    tmp = array_make(char *);
    array_traverse(plugin_arr, it) {
        array_push(tmp, (*it).plugin_name);
    }

    FN_BODY_FOR_COMPLETE_FROM_ARRAY(name, array_len(tmp), (char **)array_data(tmp), comp_res, ret);
    return ret;
}

void yed_open_buffer_window(char *buff) {
    YEXE("special-buffer-prepare-focus", buff);
    YEXE("buffer", buff);
}

int yed_plugin_boot(yed_plugin *self) {

    YED_PLUG_VERSION_CHECK();

    yed_plugin_set_command(self, "ypm-init", ypm_init);
    yed_plugin_set_command(self, "ypm-list", ypm_list);
    yed_plugin_set_command(self, "ypm-help", ypm_help);
    yed_plugin_set_command(self, "ypm-install", ypm_install);
    yed_plugin_set_command(self, "ypm-uninstall", ypm_uninstall);
    yed_plugin_set_command(self, "ypm-update", ypm_update);

    ypm_self = self;

    ypm_list_handler.kind = EVENT_PRE_PUMP;
    ypm_list_handler.fn   = list_pump_handler;
    yed_plugin_add_event_handler(self, ypm_list_handler);

    ypm_setup();
    yed_grab_plugin_names();

    yed_plugin_set_completion(self, "ypm-install-compl-arg-0", set_template_completion);
    yed_plugin_set_completion(self, "ypm-uninstall-compl-arg-0", set_template_completion);
    yed_plugin_set_completion(self, "ypm-update-compl-arg-0", set_template_completion);

    get_or_make_buffer("ypm-buff");

    return 0;
}

void ypm_setup(void) {
    array_t tmp_arr;

    char line[512];
    char str[512];
    char app[512];
    char **it;

    YEXE("plugins-add-dir", "~/.yed/ypm/plugins");

    strcpy(app, "/yedrc_plugins");
    abs_path("~/.yed/ypm", str);
    strcat(str, app);

    tmp_arr = array_make(char *);
    int flag = 1;

    FILE *fp;
    fp = fopen(str, "r");
    if(fp != NULL) {
        char *tmp_plugin;
        while( fgets(line, 512, fp ) != NULL ) {
            if(line[strlen(line)-1] == '\n') {
                line[strlen(line)-1] = 0;
            }
            YEXE("plugin-load", line);
        }
        fclose(fp);
    }
}

void ypm_init(int n_args, char **args) {
    char my_loc[512];
    char ypm_loc[512];
    char *cmd;
    char tmp[512];
    fn = NULL;

    /*setup */
    cmd = abs_path("~/.yed/ypm/ypm_init.sh", tmp);
/*     yed_cerr("%s", cmd); */
    start_event_handler();
    yed_open_buffer_window("*ypm-cmd-buff");
    yed_run_command_in_background(cmd);
}

void ypm_list(int n_args, char **args) {
    int start_row;
    plugin *it;
    char tmp_buff[512];
    char header_buff[512];
    char dash_buff[512];
    char dash_15[512];
    char dash_12[512];
    yed_buffer *cmd_buff;

    fn = NULL;
    /*find . -name '.git' | sed -r 's|/[^/]+$||' | sed -r 's|[.]||' | sed -r 's|[/]||' | sort | uniq */

    yed_grab_plugin_names();

    cmd_buff = get_or_make_buffer("ypm-buff");
    cmd_buff->flags &= ~BUFF_RD_ONLY;
    yed_buff_clear(cmd_buff);
    start_row = 1;
    yed_buff_insert_string(cmd_buff, "#### Must Run ypm_list To Update List ####", start_row, 1);
    start_row++;
    yed_buff_insert_string(cmd_buff, "", start_row, 1);
    start_row++;
    sprintf(header_buff, "%-15s| %-12s| %-12s| %-12s| %-12s| %-12s| %-12s|", "Plugin", "Downloaded", \
                                                "Compiled", "Installed", "Loaded", "Version", "Error");
/*     yed_buff_insert_string(cmd_buff, "Plugin         | Downloaded | Compiled | Installed | " \ */
/*                                         "Must_Be_Updated | Error ", start_row, 1); */
    yed_buff_insert_string(cmd_buff, header_buff, start_row, 1);
    start_row++;
    strcpy(dash_15, "---------------");
    strcpy(dash_12, "-------------");
    sprintf(dash_buff, "%15s|%12s|%12s|%12s|%12s|%12s|%12s|",
                                dash_15, dash_12, dash_12,
                                dash_12, dash_12, dash_12, dash_12);
    yed_buff_insert_string(cmd_buff, dash_buff, start_row, 1);

    array_traverse(plugin_arr, it) {
        start_row++;
        tmp_buff[0] = 0;
        sprintf(tmp_buff, "%-15s| %-12s| %-12s| %-12s| %-12s| %-12s| %-12s|",
                        (*it).plugin_name,
                        ((*it).downloaded          == 1) ? "\xE2\x9C\x93           " : "X",
                        ((*it).compiled            == 1) ? "\xE2\x9C\x93           " : "X",
                        ((*it).installed           == 1) ? "\xE2\x9C\x93           " : "X",
                        ((*it).loaded              == 1) ? "\xE2\x9C\x93           " : "X",
                        (strcmp((*it).version, "") != 0) ? (*it).version             : "X",
                        ((*it).error               == 0) ? " "                       : ((*it).error_str)
                );
        yed_attrs attr;
        attr.flags = ATTR_16;
        attr.fg = ATTR_16_RED;
        attr.bg = ATTR_16_BLACK;
        yed_set_attr(attr);
        yed_buff_insert_string(cmd_buff, tmp_buff, start_row, 1);
    }
    cmd_buff->flags |= BUFF_RD_ONLY;

    //open ypm-buff
    yed_open_buffer_window("*ypm-buff");
}

void ypm_help(int n_args, char **args) {
    fn = NULL;
    yed_open_buffer_window("*ypm-cmd-buff");
    yed_buffer *cmd_buff;
    cmd_buff = get_or_make_buffer("ypm-cmd-buff");
    cmd_buff->flags &= ~BUFF_RD_ONLY;
    yed_buff_insert_string(cmd_buff, "####- YPM, (YED Package Manager) -####", yed_buff_n_lines(cmd_buff)+1, 1);
    yed_buff_insert_string(cmd_buff, " ", yed_buff_n_lines(cmd_buff)+1, 1);
    yed_buff_insert_string(cmd_buff, "ypm-init", yed_buff_n_lines(cmd_buff)+1, 1);
    yed_buff_insert_string(cmd_buff, "            - Initializes git repos and directories, must be run before this plugin will work.", yed_buff_n_lines(cmd_buff)+1, 1);
    yed_buff_insert_string(cmd_buff, " ", yed_buff_n_lines(cmd_buff)+1, 1);
    yed_buff_insert_string(cmd_buff, "ypm-list", yed_buff_n_lines(cmd_buff)+1, 1);
    yed_buff_insert_string(cmd_buff, "            - Lists off all available plugins as well as their status.", yed_buff_n_lines(cmd_buff)+1, 1);
    yed_buff_insert_string(cmd_buff, " ", yed_buff_n_lines(cmd_buff)+1, 1);
    yed_buff_insert_string(cmd_buff, "ypm-install plugin-name", yed_buff_n_lines(cmd_buff)+1, 1);
    yed_buff_insert_string(cmd_buff, "            - Downloads, Compiles, Installs and Loads plugin.", yed_buff_n_lines(cmd_buff)+1, 1);
    yed_buff_insert_string(cmd_buff, " ", yed_buff_n_lines(cmd_buff)+1, 1);
    yed_buff_insert_string(cmd_buff, "ypm-uninstall plugin-name", yed_buff_n_lines(cmd_buff)+1, 1);
    yed_buff_insert_string(cmd_buff, "            - Deletes and Unloads plugin.", yed_buff_n_lines(cmd_buff)+1, 1);
    yed_buff_insert_string(cmd_buff, " ", yed_buff_n_lines(cmd_buff)+1, 1);
    yed_buff_insert_string(cmd_buff, "ypm-update plugin-name", yed_buff_n_lines(cmd_buff)+1, 1);
    yed_buff_insert_string(cmd_buff, "            - Updates a specfic plugin if a plugin name is given or all if ALL is given.", yed_buff_n_lines(cmd_buff)+1, 1);
}

void ypm_install_2(void) {
    fn = NULL;
    yed_buffer *cmd_buff;
    cmd_buff = get_or_make_buffer("ypm-cmd-buff");
    cmd_buff->flags &= ~BUFF_RD_ONLY;
    int loc;

    array_t tmp_arr;

    char line[512];
    char str[512];
    char app[512];
    char tmp_plugin_str[512];
    char **it;

    strcpy(app, "/yedrc_plugins");
    abs_path("~/.yed/ypm", str);
    strcat(str, app);

    tmp_arr = array_make(char *);
    int flag = 1;

    FILE *fp;
    fp = fopen(str, "r");
    if(fp != NULL) {
        char *tmp_plugin;
        while( fgets(line, 512, fp ) != NULL ) {
            if(line[strlen(line)-1] == '\n') {
                line[strlen(line)-1] = 0;
            }
/*             yed_buff_insert_string(cmd_buff, line, yed_buff_n_lines(cmd_buff)+1, 1); */
            tmp_plugin = strdup(line);
            array_push(tmp_arr, tmp_plugin);
        }
        fclose(fp);

        array_traverse(tmp_arr, it) {
            if(strcmp(*it, plugin_tmp_name) == 0) {
                flag = 0;
                break;
            }
        }
    }

    if(flag) {
        array_push(tmp_arr, plugin_tmp_name);
        tmp_plugin_str[0] = 0;
        strcat(tmp_plugin_str, "Adding ");
        strcat(tmp_plugin_str, plugin_tmp_name);
        yed_buff_insert_string(cmd_buff, tmp_plugin_str, yed_buff_n_lines(cmd_buff)+1, 1);
    }

    fp = fopen(str, "w");
    if(fp == NULL) {
        yed_cerr("Missing ~/.yed/ypm/yedrc_plugins file");
        return;
    }
    array_traverse(tmp_arr, it) {
/*         yed_buff_insert_string(cmd_buff, *it, yed_buff_n_lines(cmd_buff)+1, 1); */
        fprintf(fp, "%s\n", *it);
    }
    fclose(fp);

    yed_buff_insert_string(cmd_buff, "Plugin loaded", yed_buff_n_lines(cmd_buff)+1, 1);
    YEXE("plugin-load", plugin_tmp_name);
}

void ypm_install(int n_args, char **args) {
    if(cmd_buff_is_running) {
        yed_cerr("Can not run two ypm commands at once");
        return;
    }

    yed_buffer *ypm_cmd_buff;
    ypm_cmd_buff = get_or_make_buffer("ypm-cmd-buff");
    ypm_cmd_buff->flags &= ~BUFF_RD_ONLY;
    yed_buff_clear(ypm_cmd_buff);

    fn = ypm_install_2;
    char cmd1[512];
    char tmp[512];
    char *cmd;


    if(n_args != 1) {
        yed_cerr("Must have the plugin name");
        return;
    }

    plugin_tmp_name = strdup(args[0]);

    cmd1[0] = 0;
    strcat(cmd1, "PLUGIN=");
    strcat(cmd1, args[0]);
    cmd = abs_path("~/.yed/ypm/ypm_install.sh", tmp);
    strcat(cmd1, " ");
    strcat(cmd1, cmd);
/*     yed_cerr("%s", cmd1); */
    start_event_handler();
    yed_run_command_in_background(cmd1);

    yed_open_buffer_window("*ypm-cmd-buff");
}

void ypm_uninstall_2(void) {
    fn = NULL;
    yed_buffer *cmd_buff;
    cmd_buff = get_or_make_buffer("ypm-cmd-buff");
    cmd_buff->flags &= ~BUFF_RD_ONLY;
    int loc;

    array_t tmp_arr;

    char line[512];
    char str[512];
    char app[512];
    char tmp_plugin_str[512];
    char **it;
    int i;
    strcpy(app, "/yedrc_plugins");
    abs_path("~/.yed/ypm", str);
    strcat(str, app);

    tmp_arr = array_make(char *);
    int flag = 1;

    FILE *fp;
    fp = fopen(str, "r");
    if(fp != NULL) {
        char *tmp_plugin;
        while( fgets(line, 512, fp ) != NULL ) {
            if(line[strlen(line)-1] == '\n') {
                line[strlen(line)-1] = 0;
            }
            tmp_plugin = strdup(line);
            array_push(tmp_arr, tmp_plugin);
        }
        fclose(fp);

        i = 0;
        array_traverse(tmp_arr, it) {
            if(strcmp(*it, plugin_tmp_name) == 0) {
                tmp_plugin_str[0] = 0;
                strcat(tmp_plugin_str, "Deleting ");
                strcat(tmp_plugin_str, *it);
                yed_buff_insert_string(cmd_buff, tmp_plugin_str, yed_buff_n_lines(cmd_buff)+1, 1);
                array_delete(tmp_arr, i);
                break;
            }
            i++;
        }
    }

    fp = fopen(str, "w");
    if(fp == NULL) {
        yed_cerr("Missing ~/.yed/ypm/yedrc_plugins file");
        return;
    }
    array_traverse(tmp_arr, it) {
        fprintf(fp, "%s\n", *it);
    }
    fclose(fp);

    yed_buff_insert_string(cmd_buff, "Plugin unloaded", yed_buff_n_lines(cmd_buff)+1, 1);
    YEXE("plugin-unload", plugin_tmp_name);
}

void ypm_uninstall(int n_args, char **args) {
    if(cmd_buff_is_running) {
        yed_cerr("Can not run two ypm commands at once");
        return;
    }

    yed_buffer *ypm_cmd_buff;
    ypm_cmd_buff = get_or_make_buffer("ypm-cmd-buff");
    ypm_cmd_buff->flags &= ~BUFF_RD_ONLY;
    yed_buff_clear(ypm_cmd_buff);

    fn = ypm_uninstall_2;
    char cmd1[512];
    char tmp[512];
    char *cmd;


    if(n_args != 1) {
        yed_cerr("Must have the plugin name");
        return;
    }

    plugin_tmp_name = strdup(args[0]);

    cmd1[0] = 0;
    strcat(cmd1, "PLUGIN=");
    strcat(cmd1, args[0]);
    cmd = abs_path("~/.yed/ypm/ypm_uninstall.sh", tmp);
    strcat(cmd1, " ");
    strcat(cmd1, cmd);
    start_event_handler();
    yed_run_command_in_background(cmd1);
/*     remove from plugins dir */
    yed_open_buffer_window("*ypm-cmd-buff");
}

void ypm_update(int n_args, char **args) {
    if(cmd_buff_is_running) {
        yed_cerr("Can not run two ypm commands at once");
        return;
    }

    yed_buffer *ypm_cmd_buff;
    ypm_cmd_buff = get_or_make_buffer("ypm-cmd-buff");
    ypm_cmd_buff->flags &= ~BUFF_RD_ONLY;
    yed_buff_clear(ypm_cmd_buff);

    fn = NULL;
    char cmd1[512];
    char tmp[512];
    char *cmd;


    if(n_args != 1) {
        yed_cerr("Must have the plugin name");
        return;
    }

    cmd1[0] = 0;
    strcat(cmd1, "PLUGIN=");
    strcat(cmd1, args[0]);
    cmd = abs_path("~/.yed/ypm/ypm_update.sh", tmp);
    strcat(cmd1, " ");
    strcat(cmd1, cmd);
/*     yed_cerr("%s", cmd1); */
    start_event_handler();
    yed_run_command_in_background(cmd1);
}
