#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <sys/types.h>
#include <pwd.h>

#include <Elementary.h>

#include "edi_screens.h"
#include "edi_config.h"

#include "edi_private.h"

#define _EDI_WELCOME_PROJECT_NEW_TABLE_WIDTH 4

typedef struct _Edi_Template
{
   char *edje_path;
   char *skeleton_path;
   char *title;
   char *desc;
} Edi_Template;

typedef struct _Edi_Example
{
   char *edje_id;
   char *edje_path;
   char *example_path;
   char *title;
   char *desc;
} Edi_Example;

typedef struct _Edi_Welcome_Data {
   Evas_Object *pb;
   Evas_Object *button;
   char *dir;
   char *url;
   int status;
} Edi_Welcome_Data;

static Eina_List *_available_templates = NULL;
static Eina_List *_available_examples = NULL;

static Evas_Object *_welcome_window, *_welcome_naviframe;
static Evas_Object *_edi_new_popup;
static Evas_Object *_edi_welcome_list;
static Evas_Object *_edi_project_box;
static Evas_Object *_create_inputs[6];

static Evas_Object *_edi_create_button, *_edi_open_button;

static const char *_edi_message_path;

static void _edi_welcome_add_recent_projects(Evas_Object *);

static void
_edi_on_close_message(void *data,
                       Evas_Object *obj EINA_UNUSED,
                       void *event_info EINA_UNUSED)
{
   evas_object_del(data);
   evas_object_del(_edi_new_popup);
}

static void
_edi_on_delete_message(void *data,
                       Evas_Object *obj EINA_UNUSED,
                       void *event_info EINA_UNUSED)
{
   _edi_config_project_remove(_edi_message_path);

   evas_object_del(_edi_welcome_list);
   _edi_welcome_add_recent_projects(_edi_project_box);
   evas_object_del(data);
   evas_object_del(_edi_new_popup);
}

static void
_edi_message_open(const char *message, Eina_Bool deletable)
{
   Evas_Object *popup, *button;

   popup = elm_popup_add(_welcome_window);
   _edi_new_popup = popup;
   elm_object_part_text_set(popup, "title,text", message);

   button = elm_button_add(popup);
   elm_object_text_set(button, _("OK"));
   elm_object_part_content_set(popup, "button1", button);
   evas_object_smart_callback_add(button, "clicked",
                                  _edi_on_close_message, NULL);

   if (deletable)
     {
        button = elm_button_add(popup);
        elm_object_text_set(button, _("Delete"));
        elm_object_part_content_set(popup, "button2", button);
        evas_object_smart_callback_add(button, "clicked",
                                       _edi_on_delete_message, NULL);
     }

   evas_object_show(popup);
}

static void
_edi_welcome_project_open(const char *path, const unsigned int _edi_creating)
{
   if (!edi_open(path) && !_edi_creating)
     {
       _edi_message_path = path;
       _edi_message_open(_("That project directory no longer exists"), EINA_TRUE);
     }
   else
     evas_object_del(_welcome_window);
}

static void
_edi_welcome_project_chosen_cb(void *data,
                       Evas_Object *obj EINA_UNUSED,
                       void *event_info)
{
   evas_object_del(data);
   elm_object_disabled_set(_edi_open_button, EINA_FALSE);
   elm_object_disabled_set(_edi_create_button, EINA_FALSE);

   if (event_info)
     {
        _edi_welcome_project_open((const char*)event_info, EINA_FALSE);
     }
}

static void
_edi_welcome_choose_exit(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   evas_object_del(data);
   elm_object_disabled_set(_edi_open_button, EINA_FALSE);
   elm_object_disabled_set(_edi_create_button, EINA_FALSE);
}

static void
_edi_welcome_project_choose_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Evas_Object *win, *fs;

   elm_object_disabled_set(_edi_open_button, EINA_TRUE);
   elm_object_disabled_set(_edi_create_button, EINA_TRUE);

   elm_need_ethumb();
   elm_need_efreet();

   win = elm_win_util_standard_add("projectselector", _("Choose a Project Folder"));
   if (!win) return;

   elm_win_focus_highlight_enabled_set(win, EINA_TRUE);
   evas_object_smart_callback_add(win, "delete,request", _edi_welcome_choose_exit, win);

   fs = elm_fileselector_add(win);
   evas_object_size_hint_weight_set(fs, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(fs, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_smart_callback_add(fs, "done", _edi_welcome_project_chosen_cb, win);
   elm_win_resize_object_add(win, fs);
   evas_object_show(fs);

   elm_fileselector_expandable_set(fs, EINA_TRUE);
   elm_fileselector_folder_only_set(fs, EINA_TRUE);
   elm_fileselector_path_set(fs, eina_environment_home_get());
   elm_fileselector_sort_method_set(fs, ELM_FILESELECTOR_SORT_BY_FILENAME_ASC);

   evas_object_resize(win, 380 * elm_config_scale_get(), 260 * elm_config_scale_get());
   elm_win_center(win, EINA_TRUE, EINA_TRUE);
   evas_object_show(win);
}

static void
_edi_welcome_project_new_directory_row_add(const char *text, int row,
   Evas_Object *parent)
{
   Evas_Object *label, *input;

   label = elm_label_add(parent);
   elm_object_text_set(label, text);
   evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(parent, label, 0, row, 1, 1);
   evas_object_show(label);

   input = elm_fileselector_entry_add(parent);
   elm_object_text_set(input, _("Select folder"));
   elm_fileselector_folder_only_set(input, EINA_TRUE);
   evas_object_size_hint_weight_set(input, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(input, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(parent, input, 1, row, _EDI_WELCOME_PROJECT_NEW_TABLE_WIDTH - 1, 1);
   evas_object_show(input);

   _create_inputs[row] = input;
}

static void
_edi_welcome_project_new_input_row_add(const char *text, const char *placeholder, int row,
   Evas_Object *parent)
{
   Evas_Object *label, *input;

   label = elm_label_add(parent);
   elm_object_text_set(label, text);
   evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(parent, label, 0, row, 1, 1);
   evas_object_show(label);

   input = elm_entry_add(parent);
   elm_entry_scrollable_set(input, EINA_TRUE);
   elm_entry_single_line_set(input, EINA_TRUE);
   evas_object_size_hint_weight_set(input, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(input, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(parent, input, 1, row, _EDI_WELCOME_PROJECT_NEW_TABLE_WIDTH - 1, 1);
   evas_object_show(input);

   if (placeholder)
     {
        elm_object_text_set(input, placeholder);
     }
   _create_inputs[row] = input;
}

Edi_Template *
_edi_template_add(const char *directory, const char *file)
{
   Edi_Template *t;
   char *path = edi_path_append(directory, file);

   if (!ecore_file_exists(path))
     return NULL;

   t = malloc(sizeof(Edi_Template));
   t->title = edje_file_data_get(path, "title");
   t->desc = edje_file_data_get(path, "description");
   t->skeleton_path = edi_path_append(directory, edje_file_data_get(path, "file"));
   t->edje_path = path;

   return t;
}

static void
_edi_template_free(Edi_Template *t)
{
   if (!t)
     return;

   free(t->title);
   free(t->desc);
   free(t->edje_path);
   free(t->skeleton_path);
   free(t);
}

static void
_edi_templates_discover(const char *directory)
{
   Eina_List *files;
   char *file;

   files = ecore_file_ls(directory);
   EINA_LIST_FREE(files, file)
     {
        if (eina_str_has_extension(file, ".edj"))
          {
             Edi_Template *template = _edi_template_add(directory, file);
             if (template)
               _available_templates = eina_list_append(_available_templates, template);
          }

        free(file);
     }

   if (files)
     eina_list_free(files);
}

Edi_Example *
_edi_example_add(const char *examples, const char *group)
{
   Edi_Example *e;

   e = malloc(sizeof(Edi_Example));

   printf("EXITS %s, %s\n", edje_file_data_get(examples, "title"),
                            edje_file_data_get(examples, eina_slstr_printf("%s.title", group)));
   e->title = edje_file_data_get(examples, eina_slstr_printf("%s.title", group));
   e->desc = edje_file_data_get(examples, eina_slstr_printf("%s.description", group));
   e->example_path = edje_file_data_get(examples, eina_slstr_printf("%s.directory", group));
   e->edje_path = strdup(examples);
   e->edje_id = strdup(group);

   return e;
}

static void
_edi_example_free(Edi_Example *e)
{
   if (!e)
     return;

   free(e->title);
   free(e->desc);
   free(e->edje_path);
   free(e->edje_id);
   free(e->example_path);
   free(e);
}

static void
_edi_examples_discover(const char *directory)
{
   Eina_List *collection, *list;
   char path[PATH_MAX];
   const char *groupname;

   eina_file_path_join(path, sizeof(path), directory, "examples.edj");
   if (!ecore_file_exists(path))
     return;

   collection = edje_file_collection_list(path);
   EINA_LIST_FOREACH(collection, list, groupname)
     {
        printf("Found group %s\n", groupname);
        Edi_Example *example = _edi_example_add(path, groupname);
        if (example)
          _available_examples = eina_list_append(_available_examples, example);
     }

   edje_mmap_collection_list_free(collection);
}

static void
_edi_welcome_project_new_create_done_cb(const char *path, Eina_Bool success)
{
   Edi_Template *template;
   Edi_Example *example;

   if (!success)
     {
        ERR("Unable to create project at path %s", path);

        return;
     }

    EINA_LIST_FREE(_available_templates, template)
      _edi_template_free(template);
    EINA_LIST_FREE(_available_examples, example)
      _edi_example_free(example);

   _edi_welcome_project_open(path, EINA_TRUE);
}

static void
_edi_welcome_project_new_create_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Evas_Object *entry;
   const char *path, *name, *user, *email, *url;
   Edi_Template *template = (Edi_Template *) data;

   entry = elm_layout_content_get(_create_inputs[0], "elm.swallow.entry");
   path = elm_object_text_get(entry);
   name = elm_object_text_get(_create_inputs[1]);
   url = elm_object_text_get(_create_inputs[2]);
   user = elm_object_text_get(_create_inputs[3]);
   email = elm_object_text_get(_create_inputs[4]);

   if (template && path && path[0] && name && name[0])
     {
        edi_create_efl_project(template->skeleton_path, path, name, url, user, email,
                               _edi_welcome_project_new_create_done_cb);
     }
   else
     {
        if (path && !path[0])
          elm_object_focus_set(_create_inputs[0], EINA_TRUE);
        else if (name && !name[0])
          elm_object_focus_set(_create_inputs[1], EINA_TRUE);
     }
}

static int
_edi_welcome_user_fullname_get(const char *username, char *fullname, size_t max)
{
   struct passwd *p;
   char *pos;
   unsigned int n;

   if (!username)
     return 0;

   p = getpwnam(username);
   if (p == NULL || max == 0)
     return 0;

   pos = strchr(p->pw_gecos, ',');
   if (!pos)
     n = strlen(p->pw_gecos);
   else
     n = pos - p->pw_gecos;

   if (n == 0)
     return 0;
   if (n > max - 1)
     n = max - 1;

   memcpy(fullname, p->pw_gecos, n);
   fullname[n] = '\0';

   return 1;
}

static void
_edi_welcome_project_details(Evas_Object *naviframe, Edi_Template *template)
{
   Evas_Object *content, *button;
   Elm_Object_Item *item;
   int row = 0;
   char fullname[1024];
   char *username;

   content = elm_table_add(naviframe);
   elm_table_homogeneous_set(content, EINA_TRUE);
   evas_object_size_hint_weight_set(content, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(content);

   username = getenv("USER");
   if (!username)
     username = getenv("USERNAME");
   _edi_welcome_project_new_directory_row_add(_("Parent Path"), row++, content);
   _edi_welcome_project_new_input_row_add(_("Project Name"), NULL, row++, content);
   _edi_welcome_project_new_input_row_add(_("Project URL"), NULL, row++, content);
   if (_edi_welcome_user_fullname_get(username, fullname, sizeof(fullname)))
      _edi_welcome_project_new_input_row_add(_("Creator Name"), fullname, row++, content);
   else
      _edi_welcome_project_new_input_row_add(_("Creator Name"), username, row++, content);
   _edi_welcome_project_new_input_row_add(_("Creator Email"), NULL, row++, content);

   button = elm_button_add(content);
   elm_object_text_set(button, _("Create"));
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(button);
   elm_table_pack(content, button, _EDI_WELCOME_PROJECT_NEW_TABLE_WIDTH - 2, row, 2, 1);
   evas_object_smart_callback_add(button, "clicked", _edi_welcome_project_new_create_cb, template);

   item = elm_naviframe_item_push(naviframe, _("Create New Project"),
                                  NULL, NULL, content, NULL);
   elm_naviframe_item_title_enabled_set(item, EINA_TRUE, EINA_TRUE);
}

static void
_edi_welcome_button_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Elm_Object_Item *item;
   Edi_Template *template;
   Evas_Object *naviframe, *list = evas_object_data_get(obj, "selected");

   naviframe = (Evas_Object *) data;

   item = elm_genlist_selected_item_get(list);
   if (!item) return;

   template = elm_object_item_data_get(item);

   _edi_welcome_project_details(naviframe, template);
}

static Evas_Object *
_content_get(void *data, Evas_Object *obj, const char *source)
{
   Evas_Object *frame, *table, *image, *entry;
   Edi_Template *template = (Edi_Template *) data;
   Eina_Slstr *content;

   if (strcmp(source, "elm.swallow.content"))
     return NULL;

   frame = elm_frame_add(obj);
   elm_object_style_set(frame, "pad_medium");
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(frame);

   table = elm_table_add(obj);
   evas_object_size_hint_weight_set(table, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(table, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_padding_set(table, 5, 5);
   elm_table_homogeneous_set(table, EINA_TRUE);
   evas_object_show(table);
   elm_object_content_set(frame, table);

   image = elm_image_add(table);
   evas_object_size_hint_weight_set(image, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(image, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_min_set(image, 96 * elm_config_scale_get(), 96 * elm_config_scale_get());
   elm_image_file_set(image, template->edje_path, "logo");
   evas_object_show(image);
   elm_table_pack(table, image, 0, 0, 1, 1);

   entry = elm_entry_add(table);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_editable_set(entry, EINA_FALSE);
   elm_entry_scrollable_set(entry, EINA_FALSE);
   elm_entry_single_line_set(entry, EINA_FALSE);
   elm_entry_line_wrap_set(entry, ELM_WRAP_WORD);
   elm_table_pack(table, entry, 1, 0, 3, 1);
   evas_object_show(entry);

   content = eina_slstr_printf("<b>%s</b><br><br>%s", template->title, template->desc);
   elm_object_text_set(entry, content);

   return frame;
}

static char *
_header_text_get(void *data, Evas_Object *obj EINA_UNUSED, const char *source EINA_UNUSED)
{
   return strdup((char *)data);
}

static void
_header_del(void *data, Evas_Object *obj EINA_UNUSED)
{
   free(data);
}

static Evas_Object *
_example_content_get(void *data, Evas_Object *obj, const char *source)
{
   Evas_Object *frame, *table, *image, *entry;
   Edi_Example *example = (Edi_Example *) data;
   Eina_Slstr *content;

   if (strcmp(source, "elm.swallow.content"))
     return NULL;

   frame = elm_frame_add(obj);
   elm_object_style_set(frame, "pad_medium");
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(frame);

   table = elm_table_add(obj);
   evas_object_size_hint_weight_set(table, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(table, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_padding_set(table, 5, 5);
   elm_table_homogeneous_set(table, EINA_TRUE);
   evas_object_show(table);
   elm_object_content_set(frame, table);

   image = elm_image_add(table);
   evas_object_size_hint_weight_set(image, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(image, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_min_set(image, 96 * elm_config_scale_get(), 96 * elm_config_scale_get());
   elm_image_file_set(image, example->edje_path, example->edje_id);
   evas_object_show(image);
   elm_table_pack(table, image, 0, 0, 1, 1);

   entry = elm_entry_add(table);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_editable_set(entry, EINA_FALSE);
   elm_entry_scrollable_set(entry, EINA_FALSE);
   elm_entry_single_line_set(entry, EINA_FALSE);
   elm_entry_line_wrap_set(entry, ELM_WRAP_WORD);
   elm_table_pack(table, entry, 1, 0, 3, 1);
   evas_object_show(entry);

   content = eina_slstr_printf("<b>%s</b><br><br>%s", example->title, example->desc);
   elm_object_text_set(entry, content);

   return frame;
}

static void
_edi_welcome_project_new_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Eina_List *l;
   Evas_Object *content, *button, *naviframe;
   Evas_Object *table, *list, *rect, *hbox;
   Elm_Object_Item *item;
   Edi_Template *template;
   Edi_Example *example;
   Elm_Genlist_Item_Class *ith, *itc, *itc2;
   char path[PATH_MAX];

   naviframe = (Evas_Object *) data;

   EINA_LIST_FREE(_available_templates, template)
     _edi_template_free(template);

   snprintf(path, sizeof(path), "%s/templates", _edi_config_dir_get());
   _edi_templates_discover(PACKAGE_DATA_DIR "/templates");
   _edi_templates_discover(path);

   snprintf(path, sizeof(path), "%s/examples", _edi_config_dir_get());
   _edi_examples_discover(PACKAGE_DATA_DIR "/examples");
   _edi_examples_discover(path);

   content = elm_box_add(naviframe);
   evas_object_size_hint_weight_set(content, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(content, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(content);

   hbox = elm_box_add(content);
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_show(hbox);

   table = elm_table_add(content);
   evas_object_size_hint_weight_set(table, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(table, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(table);
   rect = evas_object_rectangle_add(table);
   evas_object_size_hint_weight_set(rect, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(rect, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_min_set(rect, 500 * elm_config_scale_get(), 300 * elm_config_scale_get());
   elm_table_pack(table, rect, 0, 0, 1, 1);

   list = elm_genlist_add(content);
   evas_object_size_hint_weight_set(list, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(list, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_genlist_mode_set(list, ELM_LIST_SCROLL);
   elm_scroller_bounce_set(list, EINA_TRUE, EINA_TRUE);
   elm_scroller_policy_set(list, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_ON);
   evas_object_size_hint_weight_set(list, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(list, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(list);
   elm_table_pack(table, list, 0, 0, 1, 1);
   elm_box_pack_end(hbox, table);

   ith = elm_genlist_item_class_new();
   ith->item_style = "group_index";
   ith->func.text_get = _header_text_get;
   ith->func.del = _header_del;
   elm_genlist_item_append(list, ith, _("Templates"), NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);

   itc = elm_genlist_item_class_new();
   itc->item_style = "full";
   itc->func.text_get = NULL;
   itc->func.content_get = _content_get;
   itc->func.state_get = NULL;
   itc->func.del = NULL;

   EINA_LIST_FOREACH(_available_templates, l, template)
     elm_genlist_item_append(list, itc, template, NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);

   elm_genlist_item_append(list, ith, _("Examples"), NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);

   itc2 = elm_genlist_item_class_new();
   itc2->item_style = "full";
   itc2->func.text_get = NULL;
   itc2->func.content_get = _example_content_get;
   itc2->func.state_get = NULL;
   itc2->func.del = NULL;
   EINA_LIST_FOREACH(_available_examples, l, example)
     {
        Elm_Widget_Item *item;

        item = elm_genlist_item_append(list, itc2, example, NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);
        elm_genlist_item_select_mode_set(item, ELM_OBJECT_SELECT_MODE_NONE);
     }

   elm_genlist_realized_items_update(list);

   elm_genlist_item_class_free(itc);

   button = elm_button_add(content);
   elm_object_text_set(button, _("Choose"));
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_data_set(button, "selected", list);
   evas_object_smart_callback_add(button, "clicked", _edi_welcome_button_clicked_cb, naviframe);
   evas_object_show(button);
   elm_box_pack_end(content, hbox);
   elm_box_pack_end(content, button);

   item = elm_naviframe_item_push(naviframe, _("Select Project Type"),
                                 NULL, NULL, content, NULL);

   elm_naviframe_item_title_enabled_set(item, EINA_TRUE, EINA_TRUE);
}

static void
_edi_welcome_clone_thread_end_cb(void *data, Ecore_Thread *thread EINA_UNUSED)
{
   Edi_Welcome_Data *wd = data;

   elm_progressbar_pulse(wd->pb, EINA_FALSE);
   evas_object_hide(wd->pb);
   elm_object_disabled_set(wd->button, EINA_FALSE);

   if (wd->status)
     _edi_message_open(_("Unable to clone project, please check URL or try again later"), EINA_FALSE);
   else
     _edi_welcome_project_open(wd->dir, EINA_FALSE);

   free(wd->dir);
   free(wd->url);

   if (!wd->status)
     free(wd);
}

static void
_edi_welcome_clone_thread_run_cb(void *data, Ecore_Thread *thread EINA_UNUSED)
{
   Edi_Welcome_Data *wd = data;

   wd->status = edi_scm_git_clone(wd->url, wd->dir);
}

static void
_edi_welcome_project_clone_click_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Evas_Object *entry;
   const char *parent, *name, *url;
   Edi_Welcome_Data *wd = data;

   url = elm_object_text_get(_create_inputs[0]);
   entry = elm_layout_content_get(_create_inputs[1], "elm.swallow.entry");
   parent = elm_object_text_get(entry);
   name = elm_object_text_get(_create_inputs[2]);

   if (!url || !url[0])
     {
        elm_object_focus_set(_create_inputs[0], EINA_TRUE);
        return;
     }

   if (!parent || !parent[0])
     {
        elm_object_focus_set(_create_inputs[1], EINA_TRUE);
        return;
     }

   if (!name || !name[0])
     {
        elm_object_focus_set(_create_inputs[2], EINA_TRUE);
        return;
     }

   wd->dir = edi_path_append(parent, name);
   wd->url = strdup(url);

   elm_object_disabled_set(wd->button, EINA_TRUE);
   elm_progressbar_pulse(wd->pb, EINA_TRUE);
   evas_object_show(wd->pb);

   ecore_thread_run(_edi_welcome_clone_thread_run_cb, _edi_welcome_clone_thread_end_cb, NULL, wd);
}

static void
_edi_welcome_project_clone_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Evas_Object *content, *pb, *button, *naviframe = data;
   Elm_Object_Item *item;
   Edi_Welcome_Data *wd;
   int row = 0;

   content = elm_table_add(naviframe);
   elm_table_homogeneous_set(content, EINA_TRUE);
   evas_object_size_hint_weight_set(content, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(content);

   _edi_welcome_project_new_input_row_add(_("Source Control URL"), NULL, row++, content);
   _edi_welcome_project_new_directory_row_add(_("Parent Path"), row++, content);
   _edi_welcome_project_new_input_row_add(_("Project Name"), NULL, row++, content);

   pb = elm_progressbar_add(content);
   evas_object_size_hint_align_set(pb, EVAS_HINT_FILL, 0.5);
   evas_object_size_hint_weight_set(pb, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_table_pack(content, pb, 0, row++, _EDI_WELCOME_PROJECT_NEW_TABLE_WIDTH, 1);
   elm_progressbar_pulse_set(pb, EINA_TRUE);

   button = elm_button_add(content);
   elm_object_text_set(button, _("Checkout"));
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(button);
   elm_table_pack(content, button, _EDI_WELCOME_PROJECT_NEW_TABLE_WIDTH - 2, row, 2, 1);

   wd = malloc(sizeof(Edi_Welcome_Data));
   wd->button = button;
   wd->pb = pb;

   evas_object_smart_callback_add(button, "clicked", _edi_welcome_project_clone_click_cb, wd);

   item = elm_naviframe_item_push(naviframe, _("Checkout Existing Project"),
                                  NULL, NULL, content, NULL);
   elm_naviframe_item_title_enabled_set(item, EINA_TRUE, EINA_TRUE);
}

static void
_edi_welcome_exit(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   evas_object_del(data);
   edi_close();
}

static void
_recent_project_mouse_down(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj,
                           void *event_info)
{
   Evas_Coord w;
   Evas_Event_Mouse_Down *ev;

   ev = event_info;
   evas_object_geometry_get(obj, NULL, NULL, &w, NULL);

   if (ev->output.x > w - 20)
     {
        _edi_config_project_remove((const char *)data);
        evas_object_del(_edi_welcome_list);
        _edi_welcome_add_recent_projects(_edi_project_box);
     }
   else
     _edi_welcome_project_open((const char *)data, EINA_FALSE);
}

static void
_edi_welcome_add_recent_projects(Evas_Object *box)
{
   Evas_Object *list, *label, *ic, *icon_button;
   Elm_Object_Item *item;
   Eina_List *listitem;
   Edi_Config_Project *project;
   char *display, *format;
   int displen;

   list = elm_list_add(box);
   _edi_welcome_list = list;
   evas_object_size_hint_weight_set(list, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(list, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_list_mode_set(list, ELM_LIST_LIMIT);

   EINA_LIST_FOREACH(_edi_config->projects, listitem, project)
     {
        format = "<align=right><color=#ffffff><b>%s:   </b></color></align>";
        displen = strlen(project->path) + strlen(format) - 1;
        display = malloc(sizeof(char) * displen);
        snprintf(display, displen, format, project->name);

        // Add an 'edit-delete' icon that can be clicked to remove a project directory
        icon_button = elm_button_add(box);
        ic = elm_icon_add(icon_button);
        elm_icon_standard_set(ic, "edit-delete");
        elm_image_resizable_set(ic, EINA_FALSE, EINA_TRUE);

        label = elm_label_add(box);
        elm_object_text_set(label, display);
        evas_object_color_set(label, 255, 255, 255, 255);
        evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
        evas_object_show(label);

        item = elm_list_item_append(list, project->path, label, ic, NULL, project->path);
        evas_object_event_callback_add(elm_list_item_object_get(item), EVAS_CALLBACK_MOUSE_DOWN,
                                       _recent_project_mouse_down, project->path);

        free(display);
     }

   elm_object_content_set(box, list);
   evas_object_show(list);
}

static Evas_Object *
_edi_welcome_button_create(const char *title, const char *icon_name,
                           Evas_Object *parent, Evas_Smart_Cb func, void *data)
{
   Evas_Object *button, *icon;

   button = elm_button_add(parent);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, 0.0);
   _edi_create_button = button;
   elm_object_text_set(button, title);
   icon = elm_icon_add(button);
   elm_icon_standard_set(icon, icon_name);
   elm_object_part_content_set(button, "icon", icon);
   evas_object_smart_callback_add(button, "clicked", func, data);
   evas_object_show(button);

   return button;
}

Evas_Object *edi_welcome_show()
{
   Evas_Object *win, *hbx, *box, *button, *frame, *image, *naviframe;
   Elm_Object_Item *item;
   char buf[PATH_MAX];

   win = elm_win_util_standard_add("main", _("Welcome to Edi"));
   if (!win) return NULL;

   _welcome_window = win;
   elm_win_focus_highlight_enabled_set(win, EINA_TRUE);
   evas_object_smart_callback_add(win, "delete,request", _edi_welcome_exit, win);

   naviframe = elm_naviframe_add(win);
   evas_object_size_hint_weight_set(naviframe, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_win_resize_object_add(win, naviframe);
   evas_object_show(naviframe);
   _welcome_naviframe = naviframe;

   hbx = elm_box_add(naviframe);
   elm_box_horizontal_set(hbx, EINA_TRUE);
   evas_object_size_hint_weight_set(hbx, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(hbx);

   /* Existing projects area */
   box = elm_box_add(hbx);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(hbx, box);
   evas_object_show(box);

   frame = elm_frame_add(box);
   elm_object_text_set(frame, _("Recent Projects"));
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, frame);
   evas_object_show(frame);

   _edi_project_box = frame;
   _edi_welcome_add_recent_projects(frame);

   button = _edi_welcome_button_create(_("Open Existing Project"), "folder",
                                       box, _edi_welcome_project_choose_cb, NULL);
   elm_box_pack_end(box, button);


   /* New project area */
   box = elm_box_add(hbx);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(hbx, box);
   evas_object_show(box);

   snprintf(buf, sizeof(buf), "%s/images/welcome.png", elm_app_data_dir_get());
   image = elm_image_add(box);
   elm_image_file_set(image, buf, NULL);
   evas_object_size_hint_weight_set(image, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(image, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, image);
   evas_object_show(image);

   button = _edi_welcome_button_create(_("Create New Project"), "folder-new",
                                       box, _edi_welcome_project_new_cb, naviframe);
   elm_box_pack_end(box, button);

   button = _edi_welcome_button_create(_("Checkout Existing Project"), "network-server",
                                       box, _edi_welcome_project_clone_cb, naviframe);
   elm_box_pack_end(box, button);

   item = elm_naviframe_item_push(naviframe,
                                _("Choose Project"),
                                NULL,
                                NULL,
                                hbx,
                                NULL);

   elm_naviframe_item_title_enabled_set(item, EINA_FALSE, EINA_FALSE);
   evas_object_resize(win, ELM_SCALE_SIZE(480), ELM_SCALE_SIZE(260));
   elm_win_center(win, EINA_TRUE, EINA_TRUE);
   evas_object_show(win);

   return win;
}

Evas_Object *
edi_welcome_create_show()
{
   Evas_Object *win;

   win = edi_welcome_show();

   _edi_welcome_project_new_cb(_welcome_naviframe, NULL, NULL);
   return win;
}
