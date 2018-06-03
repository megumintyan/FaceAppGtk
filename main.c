#include <gtk-3.0/gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>

#define ID_LENGTH 8
#define FACEAPP_URL_API "https://node-01.faceapp.io/api/v2.7/photos"

GtkBuilder *builder;
GtkImage *source;
GtkImage *filtered;
GtkNotebook *notebook;
GtkLabel *error;

static gchar *filename = NULL;

void
get_filename(GtkFileChooserButton *widget,
             gpointer             data)
{
	if(filename)
		free(filename);

	filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(widget));
	gtk_image_set_from_file(source, filename);
}

char *
get_device_id(void)
{
	char letters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	char *id = malloc(ID_LENGTH + 1);

	struct timespec spec;
	clock_gettime(CLOCK_REALTIME, &spec);
	srand(spec.tv_nsec);
	for(int i = 0; i < ID_LENGTH; i++)
		id[i] = letters[rand() % strlen(letters)];
	id[ID_LENGTH] = '\0';

	return id;
}

struct MemoryStruct{
	char *memory;
	size_t size;
};

static size_t
write_memory(void *contents, size_t size, size_t nmemb, void *userp){
	size_t realsize = size * nmemb;
	struct MemoryStruct *mem = (struct MemoryStruct *)userp;

	mem->memory = realloc(mem->memory, mem->size + realsize + 1);

	if(mem->memory == NULL) {
		/* out of memory! */
		printf("not enough memory (realloc returned NULL)\n");
		return 0;
	}

	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;

	return realsize;
}

char *
get_code(char *json)
{
	char *start = json;
	while(*start != ':')
		start++;
	start++; start++;

	char *end = start;
	while(*end != '"')
		end++;

	char *code = malloc(end - start);
	strncpy(code, start, end - start);
	code[end - start] = '\0';

	return code;
}

void
curl_request(char *url,
             char *method,
             char *deviceID,
             struct MemoryStruct *chunk)
{
	CURL *curl;
	struct curl_httppost *formpost = NULL;
	struct curl_httppost *lastptr = NULL;
	struct curl_slist *list = NULL;
	static const char buf[] = "Expect:";

	chunk->size = 0;

	curl = curl_easy_init();
	list = curl_slist_append(list, buf);

	list = curl_slist_append(list, "User-agent: FaceApp/1.0.229 (Linux; Android 4.4)");
	char headerID[64];
	sprintf(headerID, "X-FaceApp-DeviceID: %s", deviceID);
	list = curl_slist_append(list, headerID);

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)chunk);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
	curl_easy_setopt(curl, CURLOPT_URL, url);
	if(!strcmp(method, "POST")){
		curl_formadd(&formpost,
		             &lastptr,
		             CURLFORM_COPYNAME, "file",
		             CURLFORM_FILENAME, "photo",
		             CURLFORM_FILE, filename,
		             CURLFORM_END);
		curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
	}

	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	curl_slist_free_all(list);
	if(formpost)
		curl_formfree(formpost);
}

void
upload_photo(GtkWidget *widget,
             gpointer  data)
{

	GtkComboBoxText *box = data;
	char *deviceID = get_device_id();

	struct MemoryStruct chunk;
	chunk.memory = malloc(1);

	curl_request(FACEAPP_URL_API, "POST", deviceID, &chunk);

	if(strstr(chunk.memory, "\"err\"")){
		gtk_label_set_text(error, chunk.memory);
		free(deviceID);
		free(chunk.memory);
		return;
	}

	char *noncropped_filters[] = {"smile",
	                              "smile_2",
	                              "hot",
	                              "old",
	                              "young"};

	char *code = get_code(chunk.memory);
	gchar *filter_name = gtk_combo_box_text_get_active_text(box);
	char cropped[] = "1";
	for(int i = 0; i < sizeof(noncropped_filters)/sizeof(char *); i++)
		if(!strcmp(noncropped_filters[i], filter_name)){
			cropped[0] = '0';
			break;
		}

	char *get_image_url = malloc(strlen(FACEAPP_URL_API) + 1 +
	                             strlen(code) +
	                             strlen("/filters/") +
	                             strlen(filter_name) +
	                             strlen("?cropped=") + 2);
	sprintf(get_image_url, "%s/%s/filters/%s?cropped=%s", FACEAPP_URL_API, code, filter_name, cropped); 

	curl_request(get_image_url, "GET", deviceID, &chunk);

	GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
	gdk_pixbuf_loader_write (loader,
	                         chunk.memory,
	                         chunk.size,
	                         NULL);
	GdkPixbuf *pixbuf_filtered = gdk_pixbuf_loader_get_pixbuf (loader);

	gtk_image_set_from_pixbuf(filtered, pixbuf_filtered);
	gtk_notebook_set_current_page(notebook, 1);

	gdk_pixbuf_loader_close(loader, NULL);

	free(chunk.memory);
	free(deviceID);
	free(code);
}

static void
activate (GtkApplication *app,
          gpointer        user_data)
{

	GtkWidget *window;
	GtkFileChooserButton *open;
	GtkWidget *okbutton;
	GtkComboBoxText *combobox;

	builder = gtk_builder_new_from_file("FaceAppBuilder.glade");
	window = GTK_WIDGET(gtk_builder_get_object(builder, "MainWindow"));
	gtk_window_set_title(GTK_WINDOW(window), "FaceAppGtk");

	open = GTK_FILE_CHOOSER_BUTTON(gtk_builder_get_object(builder,
	                                                "ChooseImageButton"));
	source = GTK_IMAGE(gtk_builder_get_object(builder, "ImageSource"));
	filtered = GTK_IMAGE(gtk_builder_get_object(builder, "ImageFiltered"));
	okbutton = GTK_WIDGET(gtk_builder_get_object(builder, "OkButton"));
	combobox = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "ComboBox"));
	notebook = GTK_NOTEBOOK(gtk_builder_get_object(builder, "Notebook"));
	error = GTK_LABEL(gtk_builder_get_object(builder, "Error"));

	g_signal_connect(open, "file-set", G_CALLBACK(get_filename), NULL);
	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(okbutton, "clicked", G_CALLBACK(upload_photo), combobox);

	g_object_unref(builder);
	gtk_widget_show_all(window);
	gtk_main();
}

int
main (int    argc,
      char **argv)
{
	GtkApplication *app;
	int status;

	app = gtk_application_new ("org.gtk.faceapp", G_APPLICATION_FLAGS_NONE);
	g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
	status = g_application_run (G_APPLICATION (app), argc, argv);
	g_object_unref (app);

	return status;
}
