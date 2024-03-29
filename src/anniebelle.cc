//  Copyright (C) 2019  Rickard Norlander
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU Affero General Public License version 3
//  as published by the Free Software Foundation.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Affero General Public License for more details.
//
//  You should have received a copy of the GNU Affero General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <X11/XKBlib.h>
#include <cairo.h>
#include <getopt.h>
#include <gtk/gtk.h>

#if __has_include("config.h")
#include "config.h"
#else
#define PACKAGE_VERSION "unknown version"
#endif

class BellDisplayer {
  GdkPixbuf* buf;
  GtkWidget* window;

public:
  BellDisplayer(GdkPixbuf* _buf) {
    buf = _buf;
    int width = gdk_pixbuf_get_width(buf);
    int height = gdk_pixbuf_get_height(buf);

    // Key part! Tells window manager to step aside, disables
    // decorations, hides it from the list of open windows etc.
    window = gtk_window_new(GTK_WINDOW_POPUP);

    gtk_widget_set_size_request(window, width, height);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

    // Tell gtk we are going to draw everything ourselves.
    gtk_widget_set_app_paintable(window, true);
    set_visual();

    g_signal_connect(window, "draw", G_CALLBACK(draw), this);
    g_signal_connect(window, "screen-changed", G_CALLBACK(screen_changed), this);

    gtk_widget_realize(window);

    GdkWindow* gdk_window = gtk_widget_get_window(window);

    cairo_region_t* empty_region = cairo_region_create();
    gdk_window_input_shape_combine_region(
        gdk_window, empty_region, 0, 0);
    cairo_region_destroy(empty_region);
  }

  GtkWidget* get_window() {
    return window;
  }

  void set_visual() {
    // Try to enable transparency.
    GdkScreen* screen = gtk_widget_get_screen(window);
    GdkVisual* visual = gdk_screen_get_rgba_visual(screen);
    if (!visual) {
      visual = gdk_screen_get_system_visual(screen);
    }
    gtk_widget_set_visual(window, visual);
  }

  void repaint(cairo_t* cr) {
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    gdk_cairo_set_source_pixbuf(cr, buf, 0, 0);
    cairo_paint(cr);
  }

  static void screen_changed(GtkWidget* /*widget*/, GdkScreen* /*old_screen*/, gpointer userdata) {
    // We have to enable transparency again after screen changes.
    ((BellDisplayer*)userdata)->set_visual();
  }

  static gboolean draw(GtkWidget* /*widget*/, cairo_t* cr, gpointer userdata) {
   ((BellDisplayer*)userdata)->repaint(cr);
    return false;  // Propagate event.
  }
};


class BellSource {
public:
  // Has to be first member, so we can cast BellSource to a source.
  GSource source;
  Display* display;
  GtkWidget* window;
  unsigned times_shown;
  unsigned times_hidden;
  int xkb_event_type;

  static BellSource* create(Display* _display, GtkWidget* _window, int _xkb_event_type) {
    // I *think* it's fine to put this on the stack (meaning it will go away after
    // function return), but the documentation is unclear so lets be safe.
    static GSourceFuncs source_funcs = {
      nullptr,  // no prepare
      BellSource::check,
      BellSource::dispatch,
      nullptr,  // no finalize
      nullptr,  // no closure callback
      nullptr   // no closure marshal
    };
    GSource* source = g_source_new(&source_funcs, sizeof(BellSource));

    // Initialize members
    BellSource* bell_source = (BellSource*) source;
    bell_source->display = _display;
    bell_source->window = _window;
    bell_source->times_shown = 0;
    bell_source->times_hidden = 0;
    bell_source->xkb_event_type = _xkb_event_type;

    // Tell gtk to start polling for bell events.
    XkbSelectEvents(_display, XkbUseCoreKbd, XkbBellNotifyMask, XkbBellNotifyMask);
    auto xkb_fd = ConnectionNumber(_display);
    GIOCondition condition = (GIOCondition)(G_IO_IN | G_IO_HUP | G_IO_ERR);
    g_source_add_unix_fd(source, xkb_fd, condition);
    g_source_attach(source, NULL);

    return bell_source;
  }

  gboolean _check() {
    bool has_bell_event = false;
    while (XPending(display)) {
      XEvent event;
      XNextEvent(display, &event);
      if (event.type != xkb_event_type) {
	continue;
      }
      XkbEvent* xkb_event = (XkbEvent*)&event;
      if (xkb_event->any.xkb_type == XkbBellNotify) {
	has_bell_event = true;
	// Found bell event, but keep going to drain all events.
      }
    }
    return has_bell_event;
  }

  static gboolean check(GSource* source) {
    return ((BellSource*)source)->_check();
  }

  gboolean _hide() {
    ++times_hidden;
    if (times_shown == times_hidden) {
      gtk_widget_hide(window);
    }
    // Return false to request for the timer to be removed.
    return false;
  }

  static gboolean hide(gpointer user_data) {
    return ((BellSource*)user_data)->_hide();
  }


  gboolean _dispatch() {
    if (times_shown == times_hidden) {
      gtk_widget_show(window);
    }
    ++times_shown;
    g_timeout_add(200, hide, (gpointer)this);
    return true;
  }

  static gboolean dispatch(GSource* source, GSourceFunc, gpointer) {
    return ((BellSource*)source)->_dispatch();
  }
};

void usage() {
  printf("Usage: anniebelle filename\n\n"
	 "--help        Display help and exit\n"
	 "--version     Display version and exit\n");
}

int main(int argc, char** argv) {
  // Catch this before gtk_init does, so we can control the error message.
  const char* env_display = getenv("DISPLAY");
  if (env_display == nullptr || env_display[0] == '\0') {
    fprintf(stderr, "Environment variable DISPLAY is not set. Can't connect to x server\n");
    exit(1);
  }

  // Anniebelle doesn't work well under Wayland so
  // use x11 backend by default.
  const char* env_gdk_backend = getenv("GDK_BACKEND");
  if (env_gdk_backend == nullptr || env_gdk_backend[0] == '\0') {
    // Set GDK_BACKEND to x11 if unset or set to empty string.
    if (setenv("GDK_BACKEND", "x11", /*overwrite=*/1) != 0) {
      perror("Setting GDK_BACKEND=x11");
      exit(1);
    }
  }

  gtk_init(&argc, &argv);

  int option_help = 0;
  int option_version = 0;
  int option_error = 0;
  const struct option long_options[] = {
    {"help", 0, &option_help, 1},
    {"version", 0, &option_version, 1},
    {0, 0, 0, 0}
  };

  int c;
  while((c = getopt_long(argc, argv, "", long_options, nullptr)) != -1) {
    switch(c){
    case 0:
      break;
    case '?':
      option_error = true;
      break;
    }
  }

  if (option_error) {
    usage();
    return 1;
  }
  if (option_help) {
    usage();
    return 0;
  }
  if (option_version) {
    puts("anniebelle " PACKAGE_VERSION);
    return 0;
  }

  // Exactly one positional argument with the filename.
  if (optind + 1 != argc) {
    usage();
    return 1;
  }
  const char* filename = argv[optind++];

  int xkb_event_type;
  int major = XkbMajorVersion;
  int minor = XkbMinorVersion;
  Display* display = XkbOpenDisplay(nullptr, &xkb_event_type, nullptr, &major, &minor, nullptr);
  if (!display) {
    fprintf(stderr, "Failed to init xkb\n");
    return 1;
  }

  GError* error = nullptr;
  GdkPixbuf* buf = gdk_pixbuf_new_from_file(filename, &error);
  if (!buf) {
    fprintf(stderr, "Failed to load image: %s", error->message);
    g_error_free(error);
    return 1;
  }

  BellDisplayer bell_displayer(buf);
  BellSource::create(display, bell_displayer.get_window(), xkb_event_type);

  gtk_main();

  // Unreachable.
  return 0;
}
