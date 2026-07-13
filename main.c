#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CONFIG_PATH "/etc/samsung_laptop_tools.conf"

typedef struct {
    GtkWidget *entry_battery_extender;
    GtkWidget *entry_charging_threshold;
    GtkWidget *entry_intel_turbo_boost;
    GtkWidget *switch_battery_extender;
    GtkWidget *switch_battery_threshold;
    GtkWidget *switch_turbo_boost;
    GtkWidget *label_status;
    GtkWidget *btn_apply;
    
    gboolean init_battery_extender;
    double init_charging_threshold;
    gboolean init_turbo_boost;
    gboolean is_loading; 
} AppWidgets;

typedef struct {
    AppWidgets *widgets;
    int type;
} BrowseData;

int file_exists(const char *path) {
    return (path && strlen(path) > 0 && access(path, F_OK) == 0);
}

int read_int_from_file(const char *path, int default_val) {
    if (!file_exists(path)) return default_val;
    FILE *fp = fopen(path, "r");
    if (!fp) return default_val;
    int val;
    if (fscanf(fp, "%d", &val) != 1) val = default_val;
    fclose(fp);
    return val;
}

int find_sys_path(const char *filename, char *output_path, size_t max_len) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "find /sys -name \"%s\" -print -quit 2>/dev/null", filename);
    FILE *fp = popen(cmd, "r");
    if (!fp) return 0;
    if (fgets(output_path, max_len, fp) != NULL) {
        output_path[strcspn(output_path, "\n")] = '\0';
        pclose(fp);
        return (strlen(output_path) > 0); 
    }
    pclose(fp);
    return 0;
}

int load_config(char *p_extender, char *p_threshold, char *p_turbo, size_t max_len) {
    if (!file_exists(CONFIG_PATH)) return 0;

    FILE *fp = fopen(CONFIG_PATH, "r");
    if (!fp) return 0;

    char line[512];
    int count = 0;
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "BATTERY_EXTENDER=", 17) == 0) {
            strncpy(p_extender, line + 17, max_len);
            count++;
        } else if (strncmp(line, "BATTERY_THRESHOLD=", 18) == 0) {
            strncpy(p_threshold, line + 18, max_len);
            count++;
        } else if (strncmp(line, "TURBO_BOOST=", 12) == 0) {
            strncpy(p_turbo, line + 12, max_len);
            count++;
        }
    }
    fclose(fp);
    return count; 
}

void check_value_changes(AppWidgets *widgets) {
    if (!widgets || widgets->is_loading) return;
    gboolean changed = FALSE;

    if (gtk_widget_get_sensitive(widgets->switch_battery_extender)) {
        if (gtk_switch_get_active(GTK_SWITCH(widgets->switch_battery_extender)) != widgets->init_battery_extender)
            changed = TRUE;
    }
    if (gtk_widget_get_sensitive(widgets->switch_battery_threshold)) {
        if (gtk_range_get_value(GTK_RANGE(widgets->switch_battery_threshold)) != widgets->init_charging_threshold)
            changed = TRUE;
    }
    if (gtk_widget_get_sensitive(widgets->switch_turbo_boost)) {
        if (gtk_switch_get_active(GTK_SWITCH(widgets->switch_turbo_boost)) != widgets->init_turbo_boost)
            changed = TRUE;
    }

    changed |= TRUE; 

    gtk_widget_set_sensitive(widgets->btn_apply, changed);
}

static void on_state_changed(GObject *object, GParamSpec *pspec, gpointer user_data) {
    check_value_changes((AppWidgets *)user_data);
}

static void on_scale_changed(GtkRange *range, gpointer user_data) {
    check_value_changes((AppWidgets *)user_data);
}

void update_ui_from_path(AppWidgets *widgets, int type) {
    if (!widgets) return;

    if (type == 1) {
        const char *path = gtk_editable_get_text(GTK_EDITABLE(widgets->entry_battery_extender));
        if (file_exists(path)) {
            int val = read_int_from_file(path, 0);
            widgets->init_battery_extender = (val == 1);
            gtk_switch_set_active(GTK_SWITCH(widgets->switch_battery_extender), widgets->init_battery_extender);
            gtk_widget_set_sensitive(widgets->switch_battery_extender, TRUE);
        } else {
            gtk_widget_set_sensitive(widgets->switch_battery_extender, FALSE);
        }
    } 
    else if (type == 2) {
        const char *path = gtk_editable_get_text(GTK_EDITABLE(widgets->entry_charging_threshold));
        if (file_exists(path)) {
            int val = read_int_from_file(path, 100);
            widgets->init_charging_threshold = (double)val;
            gtk_range_set_value(GTK_RANGE(widgets->switch_battery_threshold), widgets->init_charging_threshold);
            gtk_widget_set_sensitive(widgets->switch_battery_threshold, TRUE);
        } else {
            gtk_widget_set_sensitive(widgets->switch_battery_threshold, FALSE);
        }
    } 
    else if (type == 3) {
        const char *path = gtk_editable_get_text(GTK_EDITABLE(widgets->entry_intel_turbo_boost));
        if (file_exists(path)) {
            int val = read_int_from_file(path, 0);
            widgets->init_turbo_boost = (val == 0);
            gtk_switch_set_active(GTK_SWITCH(widgets->switch_turbo_boost), widgets->init_turbo_boost);
            gtk_widget_set_sensitive(widgets->switch_turbo_boost, TRUE);
        } else {
            gtk_widget_set_sensitive(widgets->switch_turbo_boost, FALSE);
        }
    }
}

static void on_entry_activate(GtkEntry *entry, gpointer user_data) {
    AppWidgets *widgets = (AppWidgets *)user_data;
    if (!widgets) return;

    widgets->is_loading = TRUE;
    if (GTK_WIDGET(entry) == widgets->entry_battery_extender) update_ui_from_path(widgets, 1);
    else if (GTK_WIDGET(entry) == widgets->entry_charging_threshold) update_ui_from_path(widgets, 2);
    else if (GTK_WIDGET(entry) == widgets->entry_intel_turbo_boost) update_ui_from_path(widgets, 3);
    widgets->is_loading = FALSE;

    check_value_changes(widgets);
}

static gboolean async_detect_paths(gpointer user_data) {
    AppWidgets *widgets = (AppWidgets *)user_data;
    if (!widgets) return G_SOURCE_REMOVE;

    char p_extender[512] = "", p_threshold[512] = "", p_turbo[512] = "";
    widgets->is_loading = TRUE;

    if (load_config(p_extender, p_threshold, p_turbo, 512)) {
        gtk_editable_set_text(GTK_EDITABLE(widgets->entry_battery_extender), p_extender);
        gtk_editable_set_text(GTK_EDITABLE(widgets->entry_charging_threshold), p_threshold);
        gtk_editable_set_text(GTK_EDITABLE(widgets->entry_intel_turbo_boost), p_turbo);
        
        update_ui_from_path(widgets, 1);
        update_ui_from_path(widgets, 2);
        update_ui_from_path(widgets, 3);
        
        gtk_label_set_text(GTK_LABEL(widgets->label_status), "Configuration loaded from config file.");
    } 
    else {
        gtk_label_set_text(GTK_LABEL(widgets->label_status), "Config not found. Scanning system (First time only)...");
        
        if (find_sys_path("battery_life_extender", p_extender, sizeof(p_extender))) {
            gtk_editable_set_text(GTK_EDITABLE(widgets->entry_battery_extender), p_extender);
        }
        update_ui_from_path(widgets, 1);

        if (find_sys_path("charge_control_end_threshold", p_threshold, sizeof(p_threshold))) {
            gtk_editable_set_text(GTK_EDITABLE(widgets->entry_charging_threshold), p_threshold);
        }
        update_ui_from_path(widgets, 2);

        if (find_sys_path("no_turbo", p_turbo, sizeof(p_turbo))) {
            gtk_editable_set_text(GTK_EDITABLE(widgets->entry_intel_turbo_boost), p_turbo);
        }
        update_ui_from_path(widgets, 3);

        gtk_label_set_text(GTK_LABEL(widgets->label_status), "Scan complete.");
    }

    widgets->is_loading = FALSE;
    gtk_widget_set_sensitive(widgets->btn_apply, FALSE); 
    return G_SOURCE_REMOVE;
}

static void on_file_dialog_open_ready(GObject* source, GAsyncResult* res, gpointer data) {
    GtkFileDialog *file_dialog = GTK_FILE_DIALOG(source);
    BrowseData *bdata = (BrowseData *)data;
    if (!bdata) return;

    GFile *file = gtk_file_dialog_open_finish(file_dialog, res, NULL);
    if (file) {
        char *path = g_file_get_path(file);
        bdata->widgets->is_loading = TRUE;
        if (bdata->type == 1) gtk_editable_set_text(GTK_EDITABLE(bdata->widgets->entry_battery_extender), path);
        if (bdata->type == 2) gtk_editable_set_text(GTK_EDITABLE(bdata->widgets->entry_charging_threshold), path);
        if (bdata->type == 3) gtk_editable_set_text(GTK_EDITABLE(bdata->widgets->entry_intel_turbo_boost), path);
        update_ui_from_path(bdata->widgets, bdata->type);
        bdata->widgets->is_loading = FALSE;
        check_value_changes(bdata->widgets);
        g_free(path);
        g_object_unref(file);
    }
    g_free(bdata);
}

static void on_browse_clicked(GtkButton *btn, gpointer user_data) {
    BrowseData *bdata = (BrowseData *)user_data;
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Select Sysfs File");
    gtk_file_dialog_open(dialog, NULL, NULL, on_file_dialog_open_ready, bdata);
    g_object_unref(dialog);
}

static void on_apply_clicked(GtkButton *btn, gpointer user_data) {
    AppWidgets *widgets = (AppWidgets *)user_data;
    if (!widgets) return;

    char sub_commands[2048] = "";
    int cmd_count = 0;

    const char *path_extender = gtk_editable_get_text(GTK_EDITABLE(widgets->entry_battery_extender));
    if (file_exists(path_extender) && gtk_widget_get_sensitive(widgets->switch_battery_extender)) {
        gboolean is_on = gtk_switch_get_active(GTK_SWITCH(widgets->switch_battery_extender));
        char tmp[256];
        snprintf(tmp, sizeof(tmp), "echo %d | tee %s > /dev/null", is_on ? 1 : 0, path_extender);
        strcat(sub_commands, tmp);
        cmd_count++;
    }

    const char *path_threshold = gtk_editable_get_text(GTK_EDITABLE(widgets->entry_charging_threshold));
    if (file_exists(path_threshold) && gtk_widget_get_sensitive(widgets->switch_battery_threshold)) {
        int val = (int)gtk_range_get_value(GTK_RANGE(widgets->switch_battery_threshold));
        char tmp[256];
        if (cmd_count > 0) strcat(sub_commands, " && ");
        snprintf(tmp, sizeof(tmp), "echo %d | tee %s > /dev/null", val, path_threshold);
        strcat(sub_commands, tmp);
        cmd_count++;
    }

    const char *path_turbo = gtk_editable_get_text(GTK_EDITABLE(widgets->entry_intel_turbo_boost));
    if (file_exists(path_turbo) && gtk_widget_get_sensitive(widgets->switch_turbo_boost)) {
        gboolean is_on = gtk_switch_get_active(GTK_SWITCH(widgets->switch_turbo_boost));
        char tmp[256];
        if (cmd_count > 0) strcat(sub_commands, " && ");
        snprintf(tmp, sizeof(tmp), "echo %d | tee %s > /dev/null", is_on ? 0 : 1, path_turbo);
        strcat(sub_commands, tmp);
        cmd_count++;
    }

    if (cmd_count > 0) {
        char config_cmd[1024];
        snprintf(config_cmd, sizeof(config_cmd), 
                 " && (echo \"BATTERY_EXTENDER=%s\" && echo \"BATTERY_THRESHOLD=%s\" && echo \"TURBO_BOOST=%s\") | tee %s > /dev/null",
                 path_extender, path_threshold, path_turbo, CONFIG_PATH);
        strcat(sub_commands, config_cmd);

        char final_cmd[3000];
        snprintf(final_cmd, sizeof(final_cmd), "pkexec bash -c '%s'", sub_commands);
        
        int ret = system(final_cmd);
        if (ret == 0) {
            gtk_label_set_text(GTK_LABEL(widgets->label_status), "Settings and paths saved successfully.");
            
            if (gtk_widget_get_sensitive(widgets->switch_battery_extender))
                widgets->init_battery_extender = gtk_switch_get_active(GTK_SWITCH(widgets->switch_battery_extender));
            if (gtk_widget_get_sensitive(widgets->switch_battery_threshold))
                widgets->init_charging_threshold = gtk_range_get_value(GTK_RANGE(widgets->switch_battery_threshold));
            if (gtk_widget_get_sensitive(widgets->switch_turbo_boost))
                widgets->init_turbo_boost = gtk_switch_get_active(GTK_SWITCH(widgets->switch_turbo_boost));
            
            gtk_widget_set_sensitive(widgets->btn_apply, FALSE);
        } else {
            gtk_label_set_text(GTK_LABEL(widgets->label_status), "Authentication failed or error occurred.");
        }
    }
}

static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Samsung Laptop Tools");
    gtk_window_set_default_size(GTK_WINDOW(window), 750, 380);

    AppWidgets *widgets = g_new0(AppWidgets, 1);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_margin_start(main_box, 20);
    gtk_widget_set_margin_end(main_box, 20);
    gtk_widget_set_margin_top(main_box, 20);
    gtk_widget_set_margin_bottom(main_box, 20);
    gtk_window_set_child(GTK_WINDOW(window), main_box);

    GtkWidget *upper_frame = gtk_frame_new(" Sysfs Paths ");
    gtk_box_append(GTK_BOX(main_box), upper_frame);

    GtkWidget *path_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(path_grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(path_grid), 10);
    gtk_widget_set_margin_start(path_grid, 10);
    gtk_widget_set_margin_end(path_grid, 10);
    gtk_widget_set_margin_top(path_grid, 10);
    gtk_widget_set_margin_bottom(path_grid, 10);
    gtk_frame_set_child(GTK_FRAME(upper_frame), path_grid);

    gtk_grid_attach(GTK_GRID(path_grid), gtk_label_new("Battery Life Extender:"), 0, 0, 1, 1);
    widgets->entry_battery_extender = gtk_entry_new();
    gtk_widget_set_hexpand(widgets->entry_battery_extender, TRUE);
    g_signal_connect(widgets->entry_battery_extender, "activate", G_CALLBACK(on_entry_activate), widgets);
    gtk_grid_attach(GTK_GRID(path_grid), widgets->entry_battery_extender, 1, 0, 1, 1);
    GtkWidget *btn_b1 = gtk_button_new_with_label("Browse");
    BrowseData *bd1 = g_new0(BrowseData, 1); bd1->widgets = widgets; bd1->type = 1;
    g_signal_connect(btn_b1, "clicked", G_CALLBACK(on_browse_clicked), bd1);
    gtk_grid_attach(GTK_GRID(path_grid), btn_b1, 2, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(path_grid), gtk_label_new("Charging Threshold:"), 0, 1, 1, 1);
    widgets->entry_charging_threshold = gtk_entry_new();
    g_signal_connect(widgets->entry_charging_threshold, "activate", G_CALLBACK(on_entry_activate), widgets);
    gtk_grid_attach(GTK_GRID(path_grid), widgets->entry_charging_threshold, 1, 1, 1, 1);
    GtkWidget *btn_b2 = gtk_button_new_with_label("Browse");
    BrowseData *bd2 = g_new0(BrowseData, 1); bd2->widgets = widgets; bd2->type = 2;
    g_signal_connect(btn_b2, "clicked", G_CALLBACK(on_browse_clicked), bd2);
    gtk_grid_attach(GTK_GRID(path_grid), btn_b2, 2, 1, 1, 1);

    gtk_grid_attach(GTK_GRID(path_grid), gtk_label_new("Intel Turbo Boost:"), 0, 2, 1, 1);
    widgets->entry_intel_turbo_boost = gtk_entry_new();
    g_signal_connect(widgets->entry_intel_turbo_boost, "activate", G_CALLBACK(on_entry_activate), widgets);
    gtk_grid_attach(GTK_GRID(path_grid), widgets->entry_intel_turbo_boost, 1, 2, 1, 1);
    GtkWidget *btn_b3 = gtk_button_new_with_label("Browse");
    BrowseData *bd3 = g_new0(BrowseData, 1); bd3->widgets = widgets; bd3->type = 3;
    g_signal_connect(btn_b3, "clicked", G_CALLBACK(on_browse_clicked), bd3);
    gtk_grid_attach(GTK_GRID(path_grid), btn_b3, 2, 2, 1, 1);

    GtkWidget *lower_frame = gtk_frame_new(" Sysfs Controls");
    gtk_box_append(GTK_BOX(main_box), lower_frame);

    GtkWidget *control_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 30);
    gtk_widget_set_margin_start(control_box, 15);
    gtk_widget_set_margin_end(control_box, 15);
    gtk_widget_set_margin_top(control_box, 15);
    gtk_widget_set_margin_bottom(control_box, 15);
    gtk_widget_set_valign(control_box, GTK_ALIGN_CENTER);
    gtk_frame_set_child(GTK_FRAME(lower_frame), control_box);

    GtkWidget *box_ctrl1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_box_append(GTK_BOX(box_ctrl1), gtk_label_new("Battery Life Extender"));
    widgets->switch_battery_extender = gtk_switch_new();
    gtk_widget_set_halign(widgets->switch_battery_extender, GTK_ALIGN_CENTER);
    g_signal_connect(widgets->switch_battery_extender, "notify::active", G_CALLBACK(on_state_changed), widgets);
    gtk_box_append(GTK_BOX(box_ctrl1), widgets->switch_battery_extender);
    gtk_box_append(GTK_BOX(control_box), box_ctrl1);

    GtkWidget *box_ctrl2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_append(GTK_BOX(box_ctrl2), gtk_label_new("Charging Threshold"));
    widgets->switch_battery_threshold = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 5);
    gtk_scale_set_draw_value(GTK_SCALE(widgets->switch_battery_threshold), TRUE);
    gtk_widget_set_size_request(widgets->switch_battery_threshold, 180, -1);
    g_signal_connect(widgets->switch_battery_threshold, "value-changed", G_CALLBACK(on_scale_changed), widgets);
    gtk_box_append(GTK_BOX(box_ctrl2), widgets->switch_battery_threshold);
    gtk_widget_set_hexpand(box_ctrl2, TRUE);
    gtk_box_append(GTK_BOX(control_box), box_ctrl2);

    GtkWidget *box_ctrl3 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_box_append(GTK_BOX(box_ctrl3), gtk_label_new("Intel Turbo Boost"));
    widgets->switch_turbo_boost = gtk_switch_new();
    gtk_widget_set_halign(widgets->switch_turbo_boost, GTK_ALIGN_CENTER);
    g_signal_connect(widgets->switch_turbo_boost, "notify::active", G_CALLBACK(on_state_changed), widgets);
    gtk_box_append(GTK_BOX(box_ctrl3), widgets->switch_turbo_boost);
    gtk_box_append(GTK_BOX(control_box), box_ctrl3);

    gtk_box_append(GTK_BOX(main_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    
    GtkWidget *bottom_action_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    gtk_box_append(GTK_BOX(main_box), bottom_action_box);

    widgets->label_status = gtk_label_new("Checking saved configurations...");
    gtk_widget_set_halign(widgets->label_status, GTK_ALIGN_START);
    gtk_widget_set_valign(widgets->label_status, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(widgets->label_status, TRUE);
    gtk_box_append(GTK_BOX(bottom_action_box), widgets->label_status);

    widgets->btn_apply = gtk_button_new_with_label("Apply All Settings");
    gtk_widget_set_halign(widgets->btn_apply, GTK_ALIGN_END);
    gtk_widget_set_valign(widgets->btn_apply, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(widgets->btn_apply, 150, 40);
    gtk_widget_set_sensitive(widgets->btn_apply, FALSE);
    g_signal_connect(widgets->btn_apply, "clicked", G_CALLBACK(on_apply_clicked), widgets);
    gtk_box_append(GTK_BOX(bottom_action_box), widgets->btn_apply);

    g_idle_add(async_detect_paths, widgets);

    g_object_set_data_full(G_OBJECT(window), "app_widgets", widgets, g_free);
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.samsung.laptop_control", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
