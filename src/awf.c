/* widget-factory: a collection of widgets, for easy theme testing
 *
 * Copyright (C) 2011 Canonical Ltd
 *
 * This  library is free  software; you can  redistribute it and/or
 * modify it  under  the terms  of the  GNU Lesser  General  Public
 * License  as published  by the Free  Software  Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed  in the hope that it will be useful,
 * but  WITHOUT ANY WARRANTY; without even  the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by Andrea Cimitan <andrea.cimitan@canonical.com>
 */

// #include "config.h"

#include "glib-unix.h"
#include "glib/gprintf.h"

#include <gdk/gdk.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gtk/gtkunixprint.h>

#define GETTEXT_PACKAGE g_strdup_printf( "awf-gtk%d", GTK_MAJOR_VERSION )
#define GETTEXT_PACKAGE "awf-gtk2"
#define _app( String ) dgettext( GETTEXT_PACKAGE, String )

#define BOXH ( gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 ) )
#define BOXV ( gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 ) )
#define SEPH ( gtk_separator_new( GTK_ORIENTATION_HORIZONTAL ) )
#define SEPV ( gtk_separator_new( GTK_ORIENTATION_VERTICAL ) )
#define PANEH ( gtk_paned_new( GTK_ORIENTATION_HORIZONTAL ) )
#define PANEV ( gtk_paned_new( GTK_ORIENTATION_VERTICAL ) )

static GSList* list_system_theme = NULL;
static GSList* list_user_theme = NULL;
static GtkWidget *window = NULL, *statusbar = NULL;
// static GtkWidget *volume1 = NULL, *volume2 = NULL;
// static GtkWidget *progressbar1 = NULL, *progressbar2 = NULL, *progressbar3 = NULL, *progressbar4 = NULL;
static GtkWidget *scale1 = NULL, *scale2 = NULL, *scale3 = NULL, *scale4 = NULL, *scale5 = NULL, *scale6 = NULL;
// static GtkWidget *levelbar1 = NULL, *levelbar2 = NULL, *levelbar3 = NULL, *levelbar4 = NULL;
// static GtkWidget *levelbar5 = NULL, *levelbar6 = NULL, *levelbar7 = NULL, *levelbar8 = NULL;
// static GtkWidget *notebook1 = NULL, *notebook2 = NULL, *notebook3 = NULL, *notebook4 = NULL;
static GtkWidget* current_menuitem = NULL;
static int current_direction = GTK_TEXT_DIR_NONE;
static gchar* current_theme = "auto";
// static gchar* opt_screenshot = NULL;
// static gboolean opt_startspinner = TRUE;
static gboolean allow_update_theme = TRUE;
static gboolean must_save_accels = FALSE;

static void
    change_theme_state( GSimpleAction* action,
        GVariant* state,
        gpointer user_data ) {
  GtkSettings* settings = gtk_settings_get_default();

  g_object_set( G_OBJECT( settings ),
      "gtk-application-prefer-dark-theme",
      g_variant_get_boolean( state ),
      NULL );

  g_simple_action_set_state( action, state );
}

static GtkWidget* page_stack;

static void
    change_transition_state( GSimpleAction* action,
        GVariant* state,
        gpointer user_data ) {
  GtkStackTransitionType transition;

  if( g_variant_get_boolean( state ) )
    transition = GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT;
  else
    transition = GTK_STACK_TRANSITION_TYPE_NONE;

  gtk_stack_set_transition_type( GTK_STACK( page_stack ), transition );

  g_simple_action_set_state( action, state );
}

static gboolean
    get_idle( gpointer data ) {
  GtkWidget* window = data;
  GtkApplication* app = gtk_window_get_application( GTK_WINDOW( window ) );

  gtk_widget_set_sensitive( window, TRUE );
  gdk_window_set_cursor( gtk_widget_get_window( window ), NULL );
  g_application_unmark_busy( G_APPLICATION( app ) );

  return G_SOURCE_REMOVE;
}

static void update_statusbar( gchar* message ) {
  if( !statusbar )
    return;

  char buffer [ 12 ];
  time_t rawtime;

  time( &rawtime );
  strftime( buffer, sizeof buffer, "%T", localtime( &rawtime ) );

  gchar* text = g_strdup_printf( "%s - %s", buffer, message );
  gtk_statusbar_push( GTK_STATUSBAR( statusbar ), gtk_statusbar_get_context_id( GTK_STATUSBAR( statusbar ), "gné" ), text );
  g_free( text );
}

static GSList* awf_load_theme( gchar* directory ) {
  GSList* list = NULL;
  g_return_val_if_fail( directory != NULL, NULL );

  if( g_file_test( directory, G_FILE_TEST_IS_DIR ) ) {
    GError* error = NULL;
    GDir* dir = g_dir_open( directory, 0, &error );

    if( dir ) {
      gchar* theme = g_strdup( g_dir_read_name( dir ) );
      gchar* gtkdir = g_strdup_printf( "gtk-%d.0", GTK_MAJOR_VERSION );

      while( theme ) {
        gchar* theme_path = g_build_path( "/", directory, g_strstrip( theme ), NULL );

        if( g_file_test( theme_path, G_FILE_TEST_IS_DIR ) ) {
          gchar* theme_subpath = g_build_path( "/", theme_path, gtkdir, NULL );
          if( g_file_test( theme_subpath, G_FILE_TEST_IS_DIR ) )
            list = g_slist_prepend( list, theme );
          g_free( theme_subpath );
        }

        g_free( theme_path );
        theme = g_strdup( g_dir_read_name( dir ) );
      }

      g_dir_close( dir );
      g_free( gtkdir );
      g_free( theme );
    }

    if( error ) {
      g_fprintf( stderr, "unable to open directory: %s (%s)\n", directory, error->message );
      g_error_free( error );
    }
  }

  if( list )
    list = g_slist_reverse( list );

  return list;
}

static int awf_compare_theme( gconstpointer theme1, gconstpointer theme2 ) {
  return g_strcmp0( (gchar*) theme1, (gchar*) theme2 );
}

static void update_theme( gchar* new_theme ) {
  // we must ignore the activate signal when menubar is created
  if( !allow_update_theme )
    return;

  if( strcmp( (gchar*) new_theme, "refresh" ) == 0 ) {
    gchar* default_theme = "None";
    if( g_slist_find_custom( list_system_theme, "Default", &awf_compare_theme ) )
      default_theme = "Default";
    else if( g_slist_find_custom( list_system_theme, "Raleigh", &awf_compare_theme ) )
      default_theme = "Raleigh";

    if( default_theme ) {
      g_object_set( gtk_settings_get_default(), "gtk-theme-name", default_theme, NULL );
      g_usleep( G_USEC_PER_SEC / 2 );
      g_object_set( gtk_settings_get_default(), "gtk-theme-name", current_theme, NULL );

      // @todo force reload of ".config/gtk*/gtk.css"

      gchar* text = g_strdup_printf( _app( "Theme %s reloaded." ), current_theme );
      update_statusbar( text );
      g_free( text );
    }
  } else if( strcmp( (gchar*) new_theme, "auto" ) == 0 ) {
    g_object_get( gtk_settings_get_default(), "gtk-theme-name", &current_theme, NULL );
  } else if( strcmp( (gchar*) current_theme, (gchar*) new_theme ) != 0 ) {
    g_object_set( gtk_settings_get_default(), "gtk-theme-name", new_theme, NULL );
    g_object_get( gtk_settings_get_default(), "gtk-theme-name", &current_theme, NULL );
    // g_settings_set_string (g_settings_new ("org.mate.Marco.general"), "theme", (gchar*) current_theme);

    gtk_window_resize( GTK_WINDOW( window ), 50, 50 );

    gchar* text = g_strdup_printf( _app( "Theme %s loaded." ), current_theme );
    update_statusbar( text );
    g_free( text );
  }
}

static gboolean on_sighup() {
  update_theme( "refresh" );
  return TRUE; // glib < 2.32
}

static void
    get_busy( GSimpleAction* action,
        GVariant* parameter,
        gpointer user_data ) {
  GtkWidget* window = user_data;
  GdkCursor* cursor;
  GtkApplication* app = gtk_window_get_application( GTK_WINDOW( window ) );

  g_application_mark_busy( G_APPLICATION( app ) );
  cursor = gdk_cursor_new_from_name( gtk_widget_get_display( window ), "wait" );
  gdk_window_set_cursor( gtk_widget_get_window( window ), cursor );
  g_object_unref( cursor );
  g_timeout_add( 5000, get_idle, window );

  gtk_widget_set_sensitive( window, FALSE );
}

static gint current_page = 0;

static gboolean
    on_page( gint i ) {
  return current_page == i;
}

static void
    activate_search( GSimpleAction* action,
        GVariant* parameter,
        gpointer user_data ) {
  GtkWidget* window = user_data;
  GtkWidget* searchbar;

  if( !on_page( 2 ) )
    return;

  searchbar = GTK_WIDGET( g_object_get_data( G_OBJECT( window ), "searchbar" ) );
  gtk_search_bar_set_search_mode( GTK_SEARCH_BAR( searchbar ), TRUE );
}

static void
    activate_delete( GSimpleAction* action,
        GVariant* parameter,
        gpointer user_data ) {
  GtkWidget* window = user_data;
  GtkWidget* infobar;

  if( !on_page( 2 ) )
    return;

  infobar = GTK_WIDGET( g_object_get_data( G_OBJECT( window ), "infobar" ) );
  gtk_widget_show( infobar );
}

static void populate_flowbox( GtkWidget* flowbox );

static void
    activate_background( GSimpleAction* action,
        GVariant* parameter,
        gpointer user_data ) {
  GtkWidget* window = user_data;
  GtkWidget* dialog;
  GtkWidget* flowbox;

  if( !on_page( 2 ) )
    return;

  dialog = GTK_WIDGET( g_object_get_data( G_OBJECT( window ), "selection_dialog" ) );
  flowbox = GTK_WIDGET( g_object_get_data( G_OBJECT( window ), "selection_flowbox" ) );

  gtk_widget_show( dialog );
  populate_flowbox( flowbox );
}

static void
    activate_open( GSimpleAction* action,
        GVariant* parameter,
        gpointer user_data ) {
  GtkWidget* window = user_data;
  GtkWidget* button;

  if( !on_page( 3 ) )
    return;

  button = GTK_WIDGET( g_object_get_data( G_OBJECT( window ), "open_menubutton" ) );
  gtk_button_clicked( GTK_BUTTON( button ) );
}

static void
    activate_record( GSimpleAction* action,
        GVariant* parameter,
        gpointer user_data ) {
  GtkWidget* window = user_data;
  GtkWidget* button;

  if( !on_page( 3 ) )
    return;

  button = GTK_WIDGET( g_object_get_data( G_OBJECT( window ), "record_button" ) );
  gtk_button_clicked( GTK_BUTTON( button ) );
}

static void
    activate_lock( GSimpleAction* action,
        GVariant* parameter,
        gpointer user_data ) {
  GtkWidget* window = user_data;
  GtkWidget* button;

  if( !on_page( 3 ) )
    return;

  button = GTK_WIDGET( g_object_get_data( G_OBJECT( window ), "lockbutton" ) );
  gtk_button_clicked( GTK_BUTTON( button ) );
}

static void
    activate_about( GSimpleAction* action,
        GVariant* parameter,
        gpointer user_data ) {
  GtkApplication* app = user_data;
  const gchar* authors [] = {
      "Andrea Cimitan",
      "Cosimo Cecchi",
      NULL };
  gchar* version;

  version = g_strdup_printf( "%s\nRunning against GTK+ %d.%d.%d",
      3,
      gtk_get_major_version(),
      gtk_get_minor_version(),
      gtk_get_micro_version() );

  gtk_show_about_dialog( GTK_WINDOW( gtk_application_get_active_window( app ) ),
      "program-name", "GTK Widget Factory",
      "version", version,
      "copyright", "© 1997—2019 The GTK Team",
      "license-type", GTK_LICENSE_LGPL_2_1,
      "website", "http://www.gtk.org",
      "comments", "Program to demonstrate GTK themes and widgets",
      "authors", authors,
      "logo-icon-name", "gtk3-widget-factory",
      "title", "About GTK Widget Factory",
      NULL );

  g_free( version );
}

static void
    activate_quit( GSimpleAction* action,
        GVariant* parameter,
        gpointer user_data ) {
  GtkApplication* app = user_data;
  GtkWidget* win;
  GList *list, *next;

  list = gtk_application_get_windows( app );
  while( list ) {
    win = list->data;
    next = list->next;

    gtk_widget_destroy( GTK_WIDGET( win ) );

    list = next;
  }
}

static void
    activate_inspector( GSimpleAction* action,
        GVariant* parameter,
        gpointer user_data ) {
  gtk_window_set_interactive_debugging( TRUE );
}

static void
    spin_value_changed( GtkAdjustment* adjustment, GtkWidget* label ) {
  GtkWidget* w;
  gint v;
  gchar* text;

  v = (int) gtk_adjustment_get_value( adjustment );

  if( ( v % 3 ) == 0 ) {
    text = g_strdup_printf( "%d is a multiple of 3", v );
    gtk_label_set_label( GTK_LABEL( label ), text );
    g_free( text );
  }

  w = gtk_widget_get_ancestor( label, GTK_TYPE_REVEALER );
  gtk_revealer_set_reveal_child( GTK_REVEALER( w ), ( v % 3 ) == 0 );
}

static void
    dismiss( GtkWidget* button ) {
  GtkWidget* w;

  w = gtk_widget_get_ancestor( button, GTK_TYPE_REVEALER );
  gtk_revealer_set_reveal_child( GTK_REVEALER( w ), FALSE );
}

static void
    spin_value_reset( GtkWidget* button, GtkAdjustment* adjustment ) {
  gtk_adjustment_set_value( adjustment, 50.0 );
  dismiss( button );
}

static gint pulse_time = 250;
static gint pulse_entry_mode = 0;

static void
    remove_pulse( gpointer pulse_id ) {
  g_source_remove( GPOINTER_TO_UINT( pulse_id ) );
}

static gboolean
    pulse_it( GtkWidget* widget ) {
  guint pulse_id;

  if( GTK_IS_ENTRY( widget ) )
    gtk_entry_progress_pulse( GTK_ENTRY( widget ) );
  else
    gtk_progress_bar_pulse( GTK_PROGRESS_BAR( widget ) );

  pulse_id = g_timeout_add( pulse_time, (GSourceFunc) pulse_it, widget );
  g_object_set_data_full( G_OBJECT( widget ), "pulse_id", GUINT_TO_POINTER( pulse_id ), remove_pulse );

  return G_SOURCE_REMOVE;
}

static void
    update_pulse_time( GtkAdjustment* adjustment, GtkWidget* widget ) {
  gdouble value;
  guint pulse_id;

  value = gtk_adjustment_get_value( adjustment );

  pulse_id = GPOINTER_TO_UINT( g_object_get_data( G_OBJECT( widget ), "pulse_id" ) );

  /* vary between 50 and 450 */
  pulse_time = 50 + 4 * value;

  if( value == 100 ) {
    g_object_set_data( G_OBJECT( widget ), "pulse_id", NULL );
  } else if( value < 100 ) {
    if( pulse_id == 0 && ( GTK_IS_PROGRESS_BAR( widget ) || pulse_entry_mode % 3 == 2 ) ) {
      pulse_id = g_timeout_add( pulse_time, (GSourceFunc) pulse_it, widget );
      g_object_set_data_full( G_OBJECT( widget ), "pulse_id", GUINT_TO_POINTER( pulse_id ), remove_pulse );
    }
  }
}

static void
    on_entry_icon_release( GtkEntry* entry,
        GtkEntryIconPosition icon_pos,
        GdkEvent* event,
        gpointer user_data ) {
  if( icon_pos != GTK_ENTRY_ICON_SECONDARY )
    return;

  pulse_entry_mode++;

  if( pulse_entry_mode % 3 == 0 ) {
    g_object_set_data( G_OBJECT( entry ), "pulse_id", NULL );
    gtk_entry_set_progress_fraction( entry, 0 );
  } else if( pulse_entry_mode % 3 == 1 )
    gtk_entry_set_progress_fraction( entry, 0.25 );
  else if( pulse_entry_mode % 3 == 2 ) {
    if( pulse_time - 50 < 400 ) {
      gtk_entry_set_progress_pulse_step( entry, 0.1 );
      pulse_it( GTK_WIDGET( entry ) );
    }
  }
}

#define EPSILON ( 1e-10 )

static gboolean
    on_scale_button_query_tooltip( GtkWidget* button,
        gint x,
        gint y,
        gboolean keyboard_mode,
        GtkTooltip* tooltip,
        gpointer user_data ) {
  GtkScaleButton* scale_button = GTK_SCALE_BUTTON( button );
  GtkAdjustment* adjustment;
  gdouble val;
  gchar* str;
  AtkImage* image;

  image = ATK_IMAGE( gtk_widget_get_accessible( button ) );

  adjustment = gtk_scale_button_get_adjustment( scale_button );
  val = gtk_scale_button_get_value( scale_button );

  if( val < ( gtk_adjustment_get_lower( adjustment ) + EPSILON ) ) {
    str = g_strdup( _( "Muted" ) );
  } else if( val >= ( gtk_adjustment_get_upper( adjustment ) - EPSILON ) ) {
    str = g_strdup( _( "Full Volume" ) );
  } else {
    gint percent;

    percent = (gint) ( 100. * val / ( gtk_adjustment_get_upper( adjustment ) - gtk_adjustment_get_lower( adjustment ) ) + .5 );

    str = g_strdup_printf( C_( "volume percentage", "%d %%" ), percent );
  }

  gtk_tooltip_set_text( tooltip, str );
  atk_image_set_image_description( image, str );
  g_free( str );

  return TRUE;
}

static void
    on_scale_button_value_changed( GtkScaleButton* button,
        gdouble value,
        gpointer user_data ) {
  gtk_widget_trigger_tooltip_query( GTK_WIDGET( button ) );
}

static void
    on_record_button_toggled( GtkToggleButton* button,
        gpointer user_data ) {
  GtkStyleContext* context;

  context = gtk_widget_get_style_context( GTK_WIDGET( button ) );
  if( gtk_toggle_button_get_active( button ) )
    gtk_style_context_remove_class( context, "destructive-action" );
  else
    gtk_style_context_add_class( context, "destructive-action" );
}

static void
    on_page_combo_changed( GtkComboBox* combo,
        gpointer user_data ) {
  GtkWidget* from;
  GtkWidget* to;
  GtkWidget* print;

  from = GTK_WIDGET( g_object_get_data( G_OBJECT( combo ), "range_from_spin" ) );
  to = GTK_WIDGET( g_object_get_data( G_OBJECT( combo ), "range_to_spin" ) );
  print = GTK_WIDGET( g_object_get_data( G_OBJECT( combo ), "print_button" ) );

  switch( gtk_combo_box_get_active( combo ) ) {
    case 0: /* Range */
      gtk_widget_set_sensitive( from, TRUE );
      gtk_widget_set_sensitive( to, TRUE );
      gtk_widget_set_sensitive( print, TRUE );
      break;
    case 1: /* All */
      gtk_widget_set_sensitive( from, FALSE );
      gtk_widget_set_sensitive( to, FALSE );
      gtk_spin_button_set_value( GTK_SPIN_BUTTON( from ), 1 );
      gtk_spin_button_set_value( GTK_SPIN_BUTTON( to ), 99 );
      gtk_widget_set_sensitive( print, TRUE );
      break;
    case 2: /* Current */
      gtk_widget_set_sensitive( from, FALSE );
      gtk_widget_set_sensitive( to, FALSE );
      gtk_spin_button_set_value( GTK_SPIN_BUTTON( from ), 7 );
      gtk_spin_button_set_value( GTK_SPIN_BUTTON( to ), 7 );
      gtk_widget_set_sensitive( print, TRUE );
      break;
    case 4:
      gtk_widget_set_sensitive( from, FALSE );
      gtk_widget_set_sensitive( to, FALSE );
      gtk_widget_set_sensitive( print, FALSE );
      break;
    default:;
  }
}

static void
    on_range_from_changed( GtkSpinButton* from ) {
  GtkSpinButton* to;
  gint v1, v2;

  to = GTK_SPIN_BUTTON( g_object_get_data( G_OBJECT( from ), "range_to_spin" ) );

  v1 = gtk_spin_button_get_value_as_int( from );
  v2 = gtk_spin_button_get_value_as_int( to );

  if( v1 > v2 )
    gtk_spin_button_set_value( to, v1 );
}

static void
    on_range_to_changed( GtkSpinButton* to ) {
  GtkSpinButton* from;
  gint v1, v2;

  from = GTK_SPIN_BUTTON( g_object_get_data( G_OBJECT( to ), "range_from_spin" ) );

  v1 = gtk_spin_button_get_value_as_int( from );
  v2 = gtk_spin_button_get_value_as_int( to );

  if( v1 > v2 )
    gtk_spin_button_set_value( from, v2 );
}

static void
    update_header( GtkListBoxRow* row,
        GtkListBoxRow* before,
        gpointer data ) {
  if( before != NULL &&
      gtk_list_box_row_get_header( row ) == NULL ) {
    GtkWidget* separator;

    separator = gtk_separator_new( GTK_ORIENTATION_HORIZONTAL );
    gtk_widget_show( separator );
    gtk_list_box_row_set_header( row, separator );
  }
}

static void
    info_bar_response( GtkWidget* infobar, gint response_id ) {
  if( response_id == GTK_RESPONSE_CLOSE )
    gtk_widget_hide( infobar );
}

static void
    show_dialog( GtkWidget* button, GtkWidget* dialog ) {
  gtk_widget_show( dialog );
}

static void
    close_dialog( GtkWidget* dialog ) {
  gtk_widget_hide( dialog );
}

static void
    set_needs_attention( GtkWidget* page, gboolean needs_attention ) {
  GtkWidget* stack;

  stack = gtk_widget_get_parent( page );
  gtk_container_child_set( GTK_CONTAINER( stack ), page,
      "needs-attention", needs_attention,
      NULL );
}

static gboolean
    demand_attention( gpointer stack ) {
  GtkWidget* page;

  page = gtk_stack_get_child_by_name( GTK_STACK( stack ), "page3" );
  set_needs_attention( page, TRUE );

  return G_SOURCE_REMOVE;
}

static void
    action_dialog_button_clicked( GtkButton* button, GtkWidget* page ) {
  g_timeout_add( 1000, demand_attention, page );
}

static void
    page_changed_cb( GtkWidget* stack, GParamSpec* pspec, gpointer data ) {
  const gchar* name;
  GtkWidget* window;
  GtkWidget* page;

  if( gtk_widget_in_destruction( stack ) )
    return;

  name = gtk_stack_get_visible_child_name( GTK_STACK( stack ) );

  window = gtk_widget_get_ancestor( stack, GTK_TYPE_APPLICATION_WINDOW );
  g_object_set( gtk_application_window_get_help_overlay( GTK_APPLICATION_WINDOW( window ) ),
      "view-name", name,
      NULL );

  if( g_str_equal( name, "page1" ) )
    current_page = 1;
  else if( g_str_equal( name, "page2" ) )
    current_page = 2;
  if( g_str_equal( name, "page3" ) ) {
    current_page = 3;
    page = gtk_stack_get_visible_child( GTK_STACK( stack ) );
    set_needs_attention( GTK_WIDGET( page ), FALSE );
  }
}

static void
    populate_model( GtkTreeStore* store ) {
  GtkTreeIter iter, parent0, parent1, parent2, parent3;

  gtk_tree_store_append( store, &iter, NULL );
  gtk_tree_store_set( store, &iter,
      0, "Charlemagne",
      1, "742",
      2, "814",
      -1 );
  parent0 = iter;
  gtk_tree_store_append( store, &iter, &parent0 );
  gtk_tree_store_set( store, &iter,
      0, "Pepin the Short",
      1, "714",
      2, "768",
      -1 );
  parent1 = iter;
  gtk_tree_store_append( store, &iter, &parent1 );
  gtk_tree_store_set( store, &iter,
      0, "Charles Martel",
      1, "688",
      2, "741",
      -1 );
  parent2 = iter;
  gtk_tree_store_append( store, &iter, &parent2 );
  gtk_tree_store_set( store, &iter,
      0, "Pepin of Herstal",
      1, "635",
      2, "714",
      -1 );
  parent3 = iter;
  gtk_tree_store_append( store, &iter, &parent3 );
  gtk_tree_store_set( store, &iter,
      0, "Ansegisel",
      1, "602 or 610",
      2, "murdered before 679",
      -1 );
  gtk_tree_store_append( store, &iter, &parent3 );
  gtk_tree_store_set( store, &iter,
      0, "Begga",
      1, "615",
      2, "693",
      -1 );
  gtk_tree_store_append( store, &iter, &parent2 );
  gtk_tree_store_set( store, &iter,
      0, "Alpaida",
      -1 );
  gtk_tree_store_append( store, &iter, &parent1 );
  gtk_tree_store_set( store, &iter,
      0, "Rotrude",
      -1 );
  parent2 = iter;
  gtk_tree_store_append( store, &iter, &parent2 );
  gtk_tree_store_set( store, &iter,
      0, "Liévin de Trèves",
      -1 );
  parent3 = iter;
  gtk_tree_store_append( store, &iter, &parent3 );
  gtk_tree_store_set( store, &iter,
      0, "Guérin",
      -1 );
  gtk_tree_store_append( store, &iter, &parent3 );
  gtk_tree_store_set( store, &iter,
      0, "Gunza",
      -1 );
  gtk_tree_store_append( store, &iter, &parent2 );
  gtk_tree_store_set( store, &iter,
      0, "Willigarde de Bavière",
      -1 );
  gtk_tree_store_append( store, &iter, &parent0 );
  gtk_tree_store_set( store, &iter,
      0, "Bertrada of Laon",
      1, "710",
      2, "783",
      -1 );
  parent1 = iter;
  gtk_tree_store_append( store, &iter, &parent1 );
  gtk_tree_store_set( store, &iter,
      0, "Caribert of Laon",
      2, "before 762",
      -1 );
  parent2 = iter;
  gtk_tree_store_append( store, &iter, &parent2 );
  gtk_tree_store_set( store, &iter,
      0, "Unknown",
      -1 );
  gtk_tree_store_append( store, &iter, &parent2 );
  gtk_tree_store_set( store, &iter,
      0, "Bertrada of Prüm",
      1, "ca. 670",
      2, "after 721",
      -1 );
  gtk_tree_store_append( store, &iter, &parent1 );
  gtk_tree_store_set( store, &iter,
      0, "Gisele of Aquitaine",
      -1 );
  gtk_tree_store_append( store, &iter, NULL );
  gtk_tree_store_set( store, &iter, 3, TRUE, -1 );
  gtk_tree_store_append( store, &iter, NULL );
  gtk_tree_store_set( store, &iter,
      0, "Attila the Hun",
      1, "ca. 390",
      2, "453",
      -1 );
}

static gboolean
    row_separator_func( GtkTreeModel* model, GtkTreeIter* iter, gpointer data ) {
  gboolean is_sep;

  gtk_tree_model_get( model, iter, 3, &is_sep, -1 );

  return is_sep;
}

static void
    update_title_header( GtkListBoxRow* row,
        GtkListBoxRow* before,
        gpointer data ) {
  GtkWidget* header;
  gchar* title;

  header = gtk_list_box_row_get_header( row );
  title = (gchar*) g_object_get_data( G_OBJECT( row ), "title" );
  if( !header && title ) {
    title = g_strdup_printf( "<b>%s</b>", title );

    header = gtk_label_new( title );
    gtk_label_set_use_markup( GTK_LABEL( header ), TRUE );
    gtk_widget_set_halign( header, GTK_ALIGN_START );
    gtk_widget_set_margin_top( header, 12 );
    gtk_widget_set_margin_start( header, 6 );
    gtk_widget_set_margin_end( header, 6 );
    gtk_widget_set_margin_bottom( header, 6 );
    gtk_widget_show( header );

    gtk_list_box_row_set_header( row, header );

    g_free( title );
  }
}

static void
    overshot( GtkScrolledWindow* sw, GtkPositionType pos, GtkWidget* widget ) {
  GtkWidget *box, *row, *label, *swatch;
  GdkRGBA rgba;
  const gchar* color;
  gchar* text;
  GtkWidget* silver;
  GtkWidget* gold;

  silver = GTK_WIDGET( g_object_get_data( G_OBJECT( widget ), "Silver" ) );
  gold = GTK_WIDGET( g_object_get_data( G_OBJECT( widget ), "Gold" ) );

  if( pos == GTK_POS_TOP ) {
    if( silver ) {
      gtk_container_remove( GTK_CONTAINER( widget ), silver );
      g_object_set_data( G_OBJECT( widget ), "Silver", NULL );
    }
    if( gold ) {
      gtk_container_remove( GTK_CONTAINER( widget ), gold );
      g_object_set_data( G_OBJECT( widget ), "Gold", NULL );
    }

    return;
  }

  if( gold )
    return;
  else if( silver )
    color = "Gold";
  else
    color = "Silver";

  row = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 20 );
  text = g_strconcat( "<b>", color, "</b>", NULL );
  label = gtk_label_new( text );
  g_free( text );
  g_object_set( label,
      "use-markup", TRUE,
      "halign", GTK_ALIGN_START,
      "valign", GTK_ALIGN_CENTER,
      "margin", 6,
      "xalign", 0.0,
      NULL );
  gtk_box_pack_start( GTK_BOX( row ), label, TRUE, TRUE, 0 );
  gdk_rgba_parse( &rgba, color );
  swatch = g_object_new( g_type_from_name( "GtkColorSwatch" ),
      "rgba", &rgba,
      "selectable", FALSE,
      "halign", GTK_ALIGN_END,
      "valign", GTK_ALIGN_CENTER,
      "margin", 6,
      "height-request", 24,
      NULL );
  box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
  gtk_container_add( GTK_CONTAINER( box ), swatch );
  gtk_box_pack_start( GTK_BOX( row ), box, FALSE, FALSE, 0 );
  gtk_widget_show_all( row );
  gtk_list_box_insert( GTK_LIST_BOX( widget ), row, -1 );
  row = gtk_widget_get_parent( row );
  gtk_list_box_row_set_activatable( GTK_LIST_BOX_ROW( row ), FALSE );
  g_object_set_data( G_OBJECT( widget ), color, row );
  g_object_set_data( G_OBJECT( row ), "color", (gpointer) color );
}

static void
    rgba_changed( GtkColorChooser* chooser, GParamSpec* pspec, GtkListBox* box ) {
  gtk_list_box_select_row( box, NULL );
}

static void
    set_color( GtkListBox* box, GtkListBoxRow* row, GtkColorChooser* chooser ) {
  const char* color;
  GdkRGBA rgba;

  if( !row )
    return;

  color = (const char*) g_object_get_data( G_OBJECT( row ), "color" );

  if( !color )
    return;

  if( gdk_rgba_parse( &rgba, color ) ) {
    g_signal_handlers_block_by_func( chooser, rgba_changed, box );
    gtk_color_chooser_set_rgba( chooser, &rgba );
    g_signal_handlers_unblock_by_func( chooser, rgba_changed, box );
  }
}

static void
    populate_colors( GtkWidget* widget, GtkWidget* chooser ) {
  struct {
      const gchar* name;
      const gchar* color;
      const gchar* title;
  } colors [] = {
      {"2.5",  "#C8828C", "Red"         },
      { "5",   "#C98286", NULL          },
      { "7.5", "#C9827F", NULL          },
      { "10",  "#C98376", NULL          },
      { "2.5", "#C8856D", "Red/Yellow"  },
      { "5",   "#C58764", NULL          },
      { "7.5", "#C1895E", NULL          },
      { "10",  "#BB8C56", NULL          },
      { "2.5", "#B58F4F", "Yellow"      },
      { "5",   "#AD924B", NULL          },
      { "7.5", "#A79548", NULL          },
      { "10",  "#A09749", NULL          },
      { "2.5", "#979A4E", "Yellow/Green"},
      { "5",   "#8D9C55", NULL          },
      { "7.5", "#7F9F62", NULL          },
      { "10",  "#73A06E", NULL          },
      { "2.5", "#65A27C", "Green"       },
      { "5",   "#5CA386", NULL          },
      { "7.5", "#57A38D", NULL          },
      { "10",  "#52A394", NULL          },
      { "2.5", "#4EA39A", "Green/Blue"  },
      { "5",   "#49A3A2", NULL          },
      { "7.5", "#46A2AA", NULL          },
      { "10",  "#46A1B1", NULL          },
      { "2.5", "#49A0B8", "Blue"        },
      { "5",   "#529EBD", NULL          },
      { "7.5", "#5D9CC1", NULL          },
      { "10",  "#689AC3", NULL          },
      { "2.5", "#7597C5", "Blue/Purple" },
      { "5",   "#8095C6", NULL          },
      { "7.5", "#8D91C6", NULL          },
      { "10",  "#988EC4", NULL          },
      { "2.5", "#A08CC1", "Purple"      },
      { "5",   "#A88ABD", NULL          },
      { "7.5", "#B187B6", NULL          },
      { "10",  "#B786B0", NULL          },
      { "2.5", "#BC84A9", "Purple/Red"  },
      { "5",   "#C183A0", NULL          },
      { "7.5", "#C48299", NULL          },
      { "10",  "#C68292", NULL          }
  };

  gint i;
  GtkWidget *row, *box, *label, *swatch;
  GtkWidget* sw;
  GdkRGBA rgba;

  gtk_list_box_set_header_func( GTK_LIST_BOX( widget ), update_title_header, NULL, NULL );

  for( i = 0; i < G_N_ELEMENTS( colors ); i++ ) {
    row = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 20 );
    label = gtk_label_new( colors [ i ].name );
    g_object_set( label,
        "halign", GTK_ALIGN_START,
        "valign", GTK_ALIGN_CENTER,
        "margin", 6,
        "xalign", 0.0,
        NULL );
    gtk_box_pack_start( GTK_BOX( row ), label, TRUE, TRUE, 0 );
    gdk_rgba_parse( &rgba, colors [ i ].color );
    swatch = g_object_new( g_type_from_name( "GtkColorSwatch" ),
        "rgba", &rgba,
        "selectable", FALSE,
        "halign", GTK_ALIGN_END,
        "valign", GTK_ALIGN_CENTER,
        "margin", 6,
        "height-request", 24,
        NULL );
    box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
    gtk_container_add( GTK_CONTAINER( box ), swatch );
    gtk_box_pack_start( GTK_BOX( row ), box, FALSE, FALSE, 0 );
    gtk_widget_show_all( row );
    gtk_list_box_insert( GTK_LIST_BOX( widget ), row, -1 );
    row = gtk_widget_get_parent( row );
    gtk_list_box_row_set_activatable( GTK_LIST_BOX_ROW( row ), FALSE );
    g_object_set_data( G_OBJECT( row ), "color", (gpointer) colors [ i ].color );
    if( colors [ i ].title )
      g_object_set_data( G_OBJECT( row ), "title", (gpointer) colors [ i ].title );
  }

  g_signal_connect( widget, "row-selected", G_CALLBACK( set_color ), chooser );

  gtk_list_box_invalidate_headers( GTK_LIST_BOX( widget ) );

  sw = gtk_widget_get_ancestor( widget, GTK_TYPE_SCROLLED_WINDOW );
  g_signal_connect( sw, "edge-overshot", G_CALLBACK( overshot ), widget );
}

typedef struct {
    GtkWidget* flowbox;
    gchar* filename;
} BackgroundData;

static void
    background_loaded_cb( GObject* source,
        GAsyncResult* res,
        gpointer data ) {
  BackgroundData* bd = data;
  GtkWidget* child;
  GdkPixbuf* pixbuf;
  GError* error = NULL;

  pixbuf = gdk_pixbuf_new_from_stream_finish( res, &error );
  if( error ) {
    g_warning( "Error loading '%s': %s", bd->filename, error->message );
    g_error_free( error );
    return;
  }

  child = gtk_image_new_from_pixbuf( pixbuf );
  gtk_widget_show( child );
  gtk_flow_box_insert( GTK_FLOW_BOX( bd->flowbox ), child, -1 );
  child = gtk_widget_get_parent( child );
  g_object_set_data_full( G_OBJECT( child ), "filename", bd->filename, g_free );
  g_free( bd );
}

static void
    populate_flowbox( GtkWidget* flowbox ) {
  const gchar* location;
  GDir* dir;
  GError* error = NULL;
  const gchar* name;
  gchar* filename;
  GFile* file;
  GInputStream* stream;
  BackgroundData* bd;
  GdkPixbuf* pixbuf;
  GtkWidget* child;

  if( GPOINTER_TO_UINT( g_object_get_data( G_OBJECT( flowbox ), "populated" ) ) )
    return;

  g_object_set_data( G_OBJECT( flowbox ), "populated", GUINT_TO_POINTER( 1 ) );

  pixbuf = gdk_pixbuf_new( GDK_COLORSPACE_RGB, FALSE, 8, 110, 70 );
  gdk_pixbuf_fill( pixbuf, 0xffffffff );
  child = gtk_image_new_from_pixbuf( pixbuf );
  gtk_widget_show( child );
  gtk_flow_box_insert( GTK_FLOW_BOX( flowbox ), child, -1 );

  location = "/usr/share/backgrounds/gnome";
  dir = g_dir_open( location, 0, &error );
  if( error ) {
    g_warning( "%s", error->message );
    g_error_free( error );
    return;
  }

  while( ( name = g_dir_read_name( dir ) ) != NULL ) {
    filename = g_build_filename( location, name, NULL );
    file = g_file_new_for_path( filename );
    stream = G_INPUT_STREAM( g_file_read( file, NULL, &error ) );
    if( error ) {
      g_warning( "%s", error->message );
      g_clear_error( &error );
      g_free( filename );
    } else {
      bd = g_new( BackgroundData, 1 );
      bd->flowbox = flowbox;
      bd->filename = filename;
      gdk_pixbuf_new_from_stream_at_scale_async( stream, 110, 110, TRUE, NULL,
          background_loaded_cb, bd );
    }

    g_object_unref( file );
    g_object_unref( stream );
  }

  g_dir_close( dir );
}

static void
    row_activated( GtkListBox* box, GtkListBoxRow* row ) {
  GtkWidget* image;
  GtkWidget* dialog;

  image = (GtkWidget*) g_object_get_data( G_OBJECT( row ), "image" );
  dialog = (GtkWidget*) g_object_get_data( G_OBJECT( row ), "dialog" );

  if( image )
    if( gtk_widget_get_opacity( image ) > 0 )
      gtk_widget_set_opacity( image, 0 );
    else
      gtk_widget_set_opacity( image, 1 );
  else if( dialog )
    gtk_window_present( GTK_WINDOW( dialog ) );
}

static void
    set_accel( GtkApplication* app, GtkWidget* widget ) {
  GtkWidget* accel_label;
  const gchar* action;
  gchar** accels;
  guint key;
  GdkModifierType mods;

  accel_label = gtk_bin_get_child( GTK_BIN( widget ) );
  g_assert( GTK_IS_ACCEL_LABEL( accel_label ) );

  action = gtk_actionable_get_action_name( GTK_ACTIONABLE( widget ) );
  accels = gtk_application_get_accels_for_action( app, action );

  gtk_accelerator_parse( accels [ 0 ], &key, &mods );
  gtk_accel_label_set_accel( GTK_ACCEL_LABEL( accel_label ), key, mods );

  g_strfreev( accels );
}

typedef struct
{
    GtkTextView tv;
    cairo_surface_t* surface;
} MyTextView;

typedef GtkTextViewClass MyTextViewClass;

G_DEFINE_TYPE( MyTextView, my_text_view, GTK_TYPE_TEXT_VIEW )

static void
    my_text_view_init( MyTextView* tv ) {
}

static void
    my_tv_draw_layer( GtkTextView* widget,
        GtkTextViewLayer layer,
        cairo_t* cr ) {
  MyTextView* tv = (MyTextView*) widget;

  if( layer == GTK_TEXT_VIEW_LAYER_BELOW_TEXT && tv->surface ) {
    cairo_save( cr );
    cairo_set_source_surface( cr, tv->surface, 0.0, 0.0 );
    cairo_paint_with_alpha( cr, 0.333 );
    cairo_restore( cr );
  }
}

static void
    my_tv_finalize( GObject* object ) {
  MyTextView* tv = (MyTextView*) object;

  if( tv->surface )
    cairo_surface_destroy( tv->surface );

  G_OBJECT_CLASS( my_text_view_parent_class )->finalize( object );
}

static void
    my_text_view_class_init( MyTextViewClass* class ) {
  GtkTextViewClass* tv_class = GTK_TEXT_VIEW_CLASS( class );
  GObjectClass* o_class = G_OBJECT_CLASS( class );

  o_class->finalize = my_tv_finalize;
  tv_class->draw_layer = my_tv_draw_layer;
}

static void
    my_text_view_set_background( MyTextView* tv, const gchar* filename ) {
  GdkPixbuf* pixbuf;
  GError* error = NULL;

  if( tv->surface )
    cairo_surface_destroy( tv->surface );

  tv->surface = NULL;

  if( filename == NULL )
    return;

  pixbuf = gdk_pixbuf_new_from_file( filename, &error );
  if( error ) {
    g_warning( "%s", error->message );
    g_error_free( error );
    return;
  }

  tv->surface = gdk_cairo_surface_create_from_pixbuf( pixbuf, 1, NULL );

  g_object_unref( pixbuf );

  gtk_widget_queue_draw( GTK_WIDGET( tv ) );
}

static void
    close_selection_dialog( GtkWidget* dialog, gint response, GtkWidget* tv ) {
  GtkWidget* box;
  GtkWidget* child;
  GList* children;
  const gchar* filename;

  gtk_widget_hide( dialog );

  if( response == GTK_RESPONSE_CANCEL )
    return;

  box = gtk_dialog_get_content_area( GTK_DIALOG( dialog ) );
  children = gtk_container_get_children( GTK_CONTAINER( box ) );
  box = children->data;
  g_list_free( children );
  g_assert( GTK_IS_FLOW_BOX( box ) );
  children = gtk_flow_box_get_selected_children( GTK_FLOW_BOX( box ) );

  if( !children )
    return;

  child = children->data;
  filename = (const gchar*) g_object_get_data( G_OBJECT( child ), "filename" );

  g_list_free( children );

  my_text_view_set_background( (MyTextView*) tv, filename );
}

static void
    toggle_selection_mode( GtkSwitch* sw,
        GParamSpec* pspec,
        GtkListBox* listbox ) {
  if( gtk_switch_get_active( sw ) )
    gtk_list_box_set_selection_mode( listbox, GTK_SELECTION_SINGLE );
  else
    gtk_list_box_set_selection_mode( listbox, GTK_SELECTION_NONE );

  gtk_list_box_set_activate_on_single_click( listbox, !gtk_switch_get_active( sw ) );
}

static void
    handle_insert( GtkWidget* button, GtkWidget* textview ) {
  GtkTextBuffer* buffer;
  const gchar* id;
  const gchar* text;

  id = gtk_buildable_get_name( GTK_BUILDABLE( button ) );

  if( strcmp( id, "toolbutton1" ) == 0 )
    text = "⌘";
  else if( strcmp( id, "toolbutton2" ) == 0 )
    text = "⚽";
  else if( strcmp( id, "toolbutton3" ) == 0 )
    text = "⤢";
  else if( strcmp( id, "toolbutton4" ) == 0 )
    text = "☆";
  else
    text = "";

  buffer = gtk_text_view_get_buffer( GTK_TEXT_VIEW( textview ) );
  gtk_text_buffer_insert_at_cursor( buffer, text, -1 );
}

static void
    handle_cutcopypaste( GtkWidget* button, GtkWidget* textview ) {
  GtkTextBuffer* buffer;
  GtkClipboard* clipboard;
  const gchar* id;

  clipboard = gtk_widget_get_clipboard( textview, GDK_SELECTION_CLIPBOARD );
  buffer = gtk_text_view_get_buffer( GTK_TEXT_VIEW( textview ) );
  id = gtk_buildable_get_name( GTK_BUILDABLE( button ) );

  if( strcmp( id, "cutbutton" ) == 0 )
    gtk_text_buffer_cut_clipboard( buffer, clipboard, TRUE );
  else if( strcmp( id, "copybutton" ) == 0 )
    gtk_text_buffer_copy_clipboard( buffer, clipboard );
  else if( strcmp( id, "pastebutton" ) == 0 )
    gtk_text_buffer_paste_clipboard( buffer, clipboard, NULL, TRUE );
  else if( strcmp( id, "deletebutton" ) == 0 )
    gtk_text_buffer_delete_selection( buffer, TRUE, TRUE );
}

static void
    clipboard_owner_change( GtkClipboard* clipboard, GdkEvent* event, GtkWidget* button ) {
  const gchar* id;
  gboolean has_text;

  id = gtk_buildable_get_name( GTK_BUILDABLE( button ) );
  has_text = gtk_clipboard_wait_is_text_available( clipboard );

  if( strcmp( id, "pastebutton" ) == 0 )
    gtk_widget_set_sensitive( button, has_text );
}

static void
    textbuffer_notify_selection( GObject* object, GParamSpec* pspec, GtkWidget* button ) {
  const gchar* id;
  gboolean has_selection;

  id = gtk_buildable_get_name( GTK_BUILDABLE( button ) );
  has_selection = gtk_text_buffer_get_has_selection( GTK_TEXT_BUFFER( object ) );

  if( strcmp( id, "cutbutton" ) == 0 ||
      strcmp( id, "copybutton" ) == 0 ||
      strcmp( id, "deletebutton" ) == 0 )
    gtk_widget_set_sensitive( button, has_selection );
}

static gboolean
    osd_frame_button_press( GtkWidget* frame, GdkEventButton* event, gpointer data ) {
  GtkWidget* osd;
  gboolean visible;

  osd = g_object_get_data( G_OBJECT( frame ), "osd" );
  visible = gtk_widget_get_visible( osd );
  gtk_widget_set_visible( osd, !visible );

  return GDK_EVENT_STOP;
}

static gboolean
    page_combo_separator_func( GtkTreeModel* model,
        GtkTreeIter* iter,
        gpointer data ) {
  gchar* text;
  gboolean res;

  gtk_tree_model_get( model, iter, 0, &text, -1 );
  res = g_strcmp0( text, "-" ) == 0;
  g_free( text );

  return res;
}

static void
    activate_item( GtkWidget* item, GtkTextView* tv ) {
  const gchar* tag;
  GtkTextIter start, end;
  gboolean active;

  g_object_get( item, "active", &active, NULL );
  tag = (const gchar*) g_object_get_data( G_OBJECT( item ), "tag" );
  gtk_text_buffer_get_selection_bounds( gtk_text_view_get_buffer( tv ), &start, &end );
  if( active )
    gtk_text_buffer_apply_tag_by_name( gtk_text_view_get_buffer( tv ), tag, &start, &end );
  else
    gtk_text_buffer_remove_tag_by_name( gtk_text_view_get_buffer( tv ), tag, &start, &end );
}

static void
    add_item( GtkTextView* tv,
        GtkWidget* popup,
        const gchar* text,
        const gchar* tag,
        gboolean set ) {
  GtkWidget *item, *label;

  if( GTK_IS_MENU( popup ) ) {
    item = gtk_check_menu_item_new();
    gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( item ), set );
    g_signal_connect( item, "toggled", G_CALLBACK( activate_item ), tv );
  } else {
    item = gtk_check_button_new();
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( item ), set );
    gtk_widget_set_focus_on_click( item, FALSE );
    g_signal_connect( item, "clicked", G_CALLBACK( activate_item ), tv );
  }

  label = gtk_label_new( "" );
  gtk_label_set_xalign( GTK_LABEL( label ), 0 );
  gtk_label_set_markup( GTK_LABEL( label ), text );
  gtk_widget_show( label );
  gtk_container_add( GTK_CONTAINER( item ), label );
  g_object_set_data( G_OBJECT( item ), "tag", (gpointer) tag );
  gtk_widget_show( item );
  gtk_container_add( GTK_CONTAINER( popup ), item );
}

static void
    populate_popup( GtkTextView* tv,
        GtkWidget* popup ) {
  gboolean has_selection;
  GtkWidget* item;
  GtkTextIter start, end, iter;
  GtkTextTagTable* tags;
  GtkTextTag *bold, *italic, *underline;
  gboolean all_bold, all_italic, all_underline;

  has_selection = gtk_text_buffer_get_selection_bounds( gtk_text_view_get_buffer( tv ), &start, &end );

  if( !has_selection )
    return;

  tags = gtk_text_buffer_get_tag_table( gtk_text_view_get_buffer( tv ) );
  bold = gtk_text_tag_table_lookup( tags, "bold" );
  italic = gtk_text_tag_table_lookup( tags, "italic" );
  underline = gtk_text_tag_table_lookup( tags, "underline" );
  all_bold = TRUE;
  all_italic = TRUE;
  all_underline = TRUE;
  gtk_text_iter_assign( &iter, &start );
  while( !gtk_text_iter_equal( &iter, &end ) ) {
    all_bold &= gtk_text_iter_has_tag( &iter, bold );
    all_italic &= gtk_text_iter_has_tag( &iter, italic );
    all_underline &= gtk_text_iter_has_tag( &iter, underline );
    gtk_text_iter_forward_char( &iter );
  }

  if( GTK_IS_MENU( popup ) ) {
    item = gtk_separator_menu_item_new();
    gtk_widget_show( item );
    gtk_container_add( GTK_CONTAINER( popup ), item );
  }

  add_item( tv, popup, "<b>Bold</b>", "bold", all_bold );
  add_item( tv, popup, "<i>Italics</i>", "italic", all_italic );
  add_item( tv, popup, "<u>Underline</u>", "underline", all_underline );
}

static void
    open_popover_text_changed( GtkEntry* entry, GParamSpec* pspec, GtkWidget* button ) {
  const gchar* text;

  text = gtk_entry_get_text( entry );
  gtk_widget_set_sensitive( button, strlen( text ) > 0 );
}

static gboolean
    show_page_again( gpointer data ) {
  gtk_widget_show( GTK_WIDGET( data ) );
  return G_SOURCE_REMOVE;
}

static void
    tab_close_cb( GtkWidget* page ) {
  gtk_widget_hide( page );
  g_timeout_add( 2500, show_page_again, page );
}

typedef struct _GTestPermission GTestPermission;
typedef struct _GTestPermissionClass GTestPermissionClass;

struct _GTestPermission {
    GPermission parent;
};

struct _GTestPermissionClass {
    GPermissionClass parent_class;
};

G_DEFINE_TYPE( GTestPermission, g_test_permission, G_TYPE_PERMISSION )

static void
    g_test_permission_init( GTestPermission* test ) {
  g_permission_impl_update( G_PERMISSION( test ), TRUE, TRUE, TRUE );
}

static gboolean
    update_allowed( GPermission* permission,
        gboolean allowed ) {
  g_permission_impl_update( permission, allowed, TRUE, TRUE );

  return TRUE;
}

static gboolean
    acquire( GPermission* permission,
        GCancellable* cancellable,
        GError** error ) {
  return update_allowed( permission, TRUE );
}

static void
    acquire_async( GPermission* permission,
        GCancellable* cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data ) {
  GTask* task;

  task = g_task_new( (GObject*) permission, NULL, callback, user_data );
  g_task_return_boolean( task, update_allowed( permission, TRUE ) );
  g_object_unref( task );
}

gboolean
    acquire_finish( GPermission* permission,
        GAsyncResult* res,
        GError** error ) {
  return g_task_propagate_boolean( G_TASK( res ), error );
}

static gboolean
    release( GPermission* permission,
        GCancellable* cancellable,
        GError** error ) {
  return update_allowed( permission, FALSE );
}

static void
    release_async( GPermission* permission,
        GCancellable* cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data ) {
  GTask* task;

  task = g_task_new( (GObject*) permission, NULL, callback, user_data );
  g_task_return_boolean( task, update_allowed( permission, FALSE ) );
  g_object_unref( task );
}

gboolean
    release_finish( GPermission* permission,
        GAsyncResult* result,
        GError** error ) {
  return g_task_propagate_boolean( G_TASK( result ), error );
}

static void
    g_test_permission_class_init( GTestPermissionClass* class ) {
  GPermissionClass* permission_class = G_PERMISSION_CLASS( class );

  permission_class->acquire = acquire;
  permission_class->acquire_async = acquire_async;
  permission_class->acquire_finish = acquire_finish;

  permission_class->release = release;
  permission_class->release_async = release_async;
  permission_class->release_finish = release_finish;
}

static int icon_sizes [ 5 ];

static void
    register_icon_sizes( void ) {
  static gboolean registered;

  if( registered )
    return;

  registered = TRUE;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  icon_sizes [ 0 ] = gtk_icon_size_register( "a", 16, 16 );
  icon_sizes [ 1 ] = gtk_icon_size_register( "b", 24, 24 );
  icon_sizes [ 2 ] = gtk_icon_size_register( "c", 32, 32 );
  icon_sizes [ 3 ] = gtk_icon_size_register( "d", 48, 48 );
  icon_sizes [ 4 ] = gtk_icon_size_register( "e", 64, 64 );
  G_GNUC_END_IGNORE_DEPRECATIONS
}

static int
    find_icon_size( GtkIconSize size ) {
  gint w, h, w2, h2;
  gint i;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_icon_size_lookup( size, &w, &h );
  for( i = 0; i < G_N_ELEMENTS( icon_sizes ); i++ ) {
    gtk_icon_size_lookup( icon_sizes [ i ], &w2, &h2 );
    if( w == w2 )
      return i;
  }
  G_GNUC_END_IGNORE_DEPRECATIONS

  return 2;
}

static void
    update_buttons( GtkWidget* iv, int pos ) {
  GtkWidget* button;

  button = GTK_WIDGET( g_object_get_data( G_OBJECT( iv ), "increase_button" ) );
  gtk_widget_set_sensitive( button, pos + 1 < G_N_ELEMENTS( icon_sizes ) );
  button = GTK_WIDGET( g_object_get_data( G_OBJECT( iv ), "decrease_button" ) );
  gtk_widget_set_sensitive( button, pos > 0 );
}

static void
    increase_icon_size( GtkWidget* iv ) {
  GList* cells;
  GtkCellRendererPixbuf* cell;
  GtkIconSize size;
  int i;

  cells = gtk_cell_layout_get_cells( GTK_CELL_LAYOUT( iv ) );
  cell = cells->data;
  g_list_free( cells );

  g_object_get( cell, "stock-size", &size, NULL );

  i = find_icon_size( size );
  i = CLAMP( i + 1, 0, G_N_ELEMENTS( icon_sizes ) - 1 );
  size = icon_sizes [ i ];

  g_object_set( cell, "stock-size", size, NULL );

  update_buttons( iv, i );

  gtk_widget_queue_resize( iv );
}

static void
    decrease_icon_size( GtkWidget* iv ) {
  GList* cells;
  GtkCellRendererPixbuf* cell;
  GtkIconSize size;
  int i;

  cells = gtk_cell_layout_get_cells( GTK_CELL_LAYOUT( iv ) );
  cell = cells->data;
  g_list_free( cells );

  g_object_get( cell, "stock-size", &size, NULL );

  i = find_icon_size( size );
  i = CLAMP( i - 1, 0, G_N_ELEMENTS( icon_sizes ) - 1 );
  size = icon_sizes [ i ];

  g_object_set( cell, "stock-size", size, NULL );

  update_buttons( iv, i );

  gtk_widget_queue_resize( iv );
}

static void
    reset_icon_size( GtkWidget* iv ) {
  GList* cells;
  GtkCellRendererPixbuf* cell;

  cells = gtk_cell_layout_get_cells( GTK_CELL_LAYOUT( iv ) );
  cell = cells->data;
  g_list_free( cells );

  g_object_set( cell, "stock-size", icon_sizes [ 2 ], NULL );

  update_buttons( iv, 2 );

  gtk_widget_queue_resize( iv );
}

static gchar*
    scale_format_value_blank( GtkScale* scale, gdouble value ) {
  return g_strdup( " " );
}

static gchar*
    scale_format_value( GtkScale* scale, gdouble value ) {
  return g_strdup_printf( "%0.*f", 1, value );
}

static void
    adjustment3_value_changed( GtkAdjustment* adj, GtkProgressBar* pbar ) {
  double fraction;

  fraction = gtk_adjustment_get_value( adj ) / ( gtk_adjustment_get_upper( adj ) - gtk_adjustment_get_lower( adj ) );

  gtk_progress_bar_set_fraction( pbar, fraction );
}

static GtkWidget* create_horizontal_scale( gdouble value, gboolean draw, gboolean inverted, int position ) {
  GtkWidget* scale;

#if GTK_CHECK_VERSION( 3, 0, 0 )
  scale = gtk_scale_new_with_range( GTK_ORIENTATION_HORIZONTAL, 0, 100, 1 );
#else
  scale = gtk_hscale_new_with_range( 0, 100, 1 );
#endif

  gtk_range_set_value( GTK_RANGE( scale ), value );
  gtk_scale_set_draw_value( GTK_SCALE( scale ), draw );
  gtk_scale_set_value_pos( GTK_SCALE( scale ), position );
  gtk_range_set_inverted( GTK_RANGE( scale ), inverted );
  gtk_widget_set_size_request( scale, 186, -1 );

  return scale;
}

static GtkWidget* create_vertical_scale( gdouble value, gboolean draw, gboolean inverted, int position ) {
  GtkWidget* scale;

#if GTK_CHECK_VERSION( 3, 0, 0 )
  scale = gtk_scale_new_with_range( GTK_ORIENTATION_VERTICAL, 0, 100, 1 );
#else
  scale = gtk_vscale_new_with_range( 0, 100, 1 );
#endif

  gtk_scale_set_value_pos( GTK_SCALE( scale ), position );
  gtk_range_set_inverted( GTK_RANGE( scale ), inverted );
  gtk_scale_set_draw_value( GTK_SCALE( scale ), draw );
  gtk_range_set_value( GTK_RANGE( scale ), value );
  gtk_widget_set_size_request( scale, -1, 100 );

  return scale;
}

static void
    validate_more_details( GtkEntry* entry,
        GParamSpec* pspec,
        GtkEntry* details ) {
  if( strlen( gtk_entry_get_text( entry ) ) > 0 &&
      strlen( gtk_entry_get_text( details ) ) == 0 ) {
    gtk_widget_set_tooltip_text( GTK_WIDGET( entry ), "Must have details first" );
    gtk_style_context_add_class( gtk_widget_get_style_context( GTK_WIDGET( entry ) ), "error" );
  } else {
    gtk_widget_set_tooltip_text( GTK_WIDGET( entry ), "" );
    gtk_style_context_remove_class( gtk_widget_get_style_context( GTK_WIDGET( entry ) ), "error" );
  }
}

static gboolean
    mode_switch_state_set( GtkSwitch* sw, gboolean state ) {
  GtkWidget* dialog = gtk_widget_get_ancestor( GTK_WIDGET( sw ), GTK_TYPE_DIALOG );
  GtkWidget* scale = GTK_WIDGET( g_object_get_data( G_OBJECT( dialog ), "level_scale" ) );
  GtkWidget* label = GTK_WIDGET( g_object_get_data( G_OBJECT( dialog ), "error_label" ) );

  if( !state ||
      ( gtk_range_get_value( GTK_RANGE( scale ) ) > 50 ) ) {
    gtk_widget_hide( label );
    gtk_switch_set_state( sw, state );
  } else {
    gtk_widget_show( label );
  }

  return TRUE;
}

static void
    level_scale_value_changed( GtkRange* range ) {
  GtkWidget* dialog = gtk_widget_get_ancestor( GTK_WIDGET( range ), GTK_TYPE_DIALOG );
  GtkWidget* sw = GTK_WIDGET( g_object_get_data( G_OBJECT( dialog ), "mode_switch" ) );
  GtkWidget* label = GTK_WIDGET( g_object_get_data( G_OBJECT( dialog ), "error_label" ) );

  if( gtk_switch_get_active( GTK_SWITCH( sw ) ) &&
      !gtk_switch_get_state( GTK_SWITCH( sw ) ) &&
      ( gtk_range_get_value( range ) > 50 ) ) {
    gtk_widget_hide( label );
    gtk_switch_set_state( GTK_SWITCH( sw ), TRUE );
  } else if( gtk_switch_get_state( GTK_SWITCH( sw ) ) &&
             ( gtk_range_get_value( range ) <= 50 ) ) {
    gtk_switch_set_state( GTK_SWITCH( sw ), FALSE );
  }
}

static void dialog_open() {
  GtkWidget* dialog = gtk_file_chooser_dialog_new(
      "GtkFileChooserDialog",
      GTK_WINDOW( window ),
      GTK_FILE_CHOOSER_ACTION_OPEN,
      "gtk-cancel",
      GTK_RESPONSE_CANCEL,
      "gtk-open",
      GTK_RESPONSE_ACCEPT,
      NULL );

  gtk_window_set_icon_name( GTK_WINDOW( dialog ), GETTEXT_PACKAGE );
  gtk_dialog_run( GTK_DIALOG( dialog ) );
  gtk_widget_destroy( dialog );
}

static void dialog_recent() {
  GtkWidget* dialog = gtk_recent_chooser_dialog_new(
      "GtkRecentChooserDialog",
      GTK_WINDOW( window ),
      "gtk-cancel",
      GTK_RESPONSE_CANCEL,
      "gtk-open",
      GTK_RESPONSE_ACCEPT,
      NULL );

  gtk_window_set_icon_name( GTK_WINDOW( dialog ), GETTEXT_PACKAGE );
  gtk_dialog_run( GTK_DIALOG( dialog ) );
  gtk_widget_destroy( dialog );
}

static void update_marks( GtkScale* scale, gboolean value, int position ) {
  if( value ) {
    gtk_scale_add_mark( scale, 0, position, NULL );
    gtk_scale_add_mark( scale, 25, position, "25" );
    gtk_scale_add_mark( scale, 50, position, "50" );
    gtk_scale_add_mark( scale, 75, position, "75" );
    gtk_scale_add_mark( scale, 100, position, NULL );
  } else {
    gtk_scale_add_mark( scale, 0, position, NULL );
    gtk_scale_add_mark( scale, 25, position, NULL );
    gtk_scale_add_mark( scale, 50, position, NULL );
    gtk_scale_add_mark( scale, 75, position, NULL );
    gtk_scale_add_mark( scale, 100, position, NULL );
  }
}

static void add_to( GtkBox* box, GtkWidget* widget, gboolean fill, gboolean expand, guint padding, guint spacing ) {
  if( GTK_IS_INFO_BAR( box ) )
    box = GTK_BOX( gtk_info_bar_get_content_area( GTK_INFO_BAR( box ) ) );
  if( GTK_IS_CONTAINER( widget ) && ( padding > 0 ) )
    gtk_container_set_border_width( GTK_CONTAINER( widget ), padding );
  if( GTK_IS_BOX( widget ) && ( spacing > 0 ) )
    gtk_box_set_spacing( GTK_BOX( widget ), spacing );
  gtk_box_pack_start( box, widget, fill, expand, 0 );

  if( !GTK_IS_BOX( widget ) ) {
    GtkStyleContext* style = gtk_widget_get_style_context( widget );
    gchar* text = gtk_style_context_to_string( style, GTK_STYLE_CONTEXT_PRINT_RECURSE );
    gtk_widget_set_tooltip_text( widget, text );
    g_free( text );
  }
}

static void dialog_print() {
  GtkWidget* dialog = gtk_print_unix_dialog_new( "GtkPrintUnixDialog", GTK_WINDOW( window ) );
  gtk_window_set_icon_name( GTK_WINDOW( dialog ), GETTEXT_PACKAGE );

  gtk_dialog_run( GTK_DIALOG( dialog ) );
  gtk_widget_destroy( dialog );
}

static void dialog_scales( int position ) {
  GtkWidget *dialog, *area, *hbox = BOXH;
  GtkWidget *vbox1 = BOXV, *vbox2 = BOXV, *vbox3 = BOXV, *hboxa = BOXH, *hboxb = BOXH;
  GtkWidget *scale1v, *scale2v, *scale3v, *scale4v, *scale5v, *scale6v, *scale7v, *scale8v, *scale9v, *scale10v, *scale11v, *scale12v;
  GtkWidget *scale1h, *scale2h, *scale3h, *scale4h, *scale5h, *scale6h, *scale7h, *scale8h, *scale9h, *scale10h, *scale11h, *scale12h;
  gdouble value = gtk_range_get_value( GTK_RANGE( scale1 ) );

  dialog = gtk_dialog_new_with_buttons(
      NULL,
      GTK_WINDOW( window ),
      GTK_DIALOG_DESTROY_WITH_PARENT,
      NULL,
      NULL );

  // vertical scales
  scale1v = create_vertical_scale( value, FALSE, FALSE, position );

  if( gtk_widget_get_direction( window ) == GTK_TEXT_DIR_RTL ) // @todo highlight bottom instead of top, why?
    gtk_style_context_add_class( gtk_widget_get_style_context( scale1v ), "top" );

  scale2v = create_vertical_scale( value, FALSE, FALSE, position );
  update_marks( GTK_SCALE( scale2v ), FALSE, GTK_POS_LEFT );

  if( gtk_widget_get_direction( window ) == GTK_TEXT_DIR_RTL ) // @todo highlight bottom instead of top, why?
    gtk_style_context_add_class( gtk_widget_get_style_context( scale2v ), "top" );

  scale3v = create_vertical_scale( value, FALSE, FALSE, position );
  update_marks( GTK_SCALE( scale3v ), TRUE, GTK_POS_LEFT );

  if( gtk_widget_get_direction( window ) == GTK_TEXT_DIR_RTL ) // @todo highlight bottom instead of top, why?
    gtk_style_context_add_class( gtk_widget_get_style_context( scale3v ), "top" );

  scale4v = create_vertical_scale( value, TRUE, FALSE, position );

  if( gtk_widget_get_direction( window ) == GTK_TEXT_DIR_RTL ) // @todo highlight bottom instead of top, why?
    gtk_style_context_add_class( gtk_widget_get_style_context( scale4v ), "top" );

  scale5v = create_vertical_scale( value, TRUE, FALSE, position );
  update_marks( GTK_SCALE( scale5v ), FALSE, GTK_POS_LEFT );
  if( gtk_widget_get_direction( window ) == GTK_TEXT_DIR_RTL ) // @todo highlight bottom instead of top, why?
    gtk_style_context_add_class( gtk_widget_get_style_context( scale5v ), "top" );

  scale6v = create_vertical_scale( value, TRUE, FALSE, position );
  update_marks( GTK_SCALE( scale6v ), TRUE, GTK_POS_LEFT );

  if( gtk_widget_get_direction( window ) == GTK_TEXT_DIR_RTL ) // @todo highlight bottom instead of top, why?
    gtk_style_context_add_class( gtk_widget_get_style_context( scale6v ), "top" );

  scale7v = create_vertical_scale( value, FALSE, TRUE, position );

  scale8v = create_vertical_scale( value, FALSE, TRUE, position );
  update_marks( GTK_SCALE( scale8v ), FALSE, GTK_POS_RIGHT );

  scale9v = create_vertical_scale( value, FALSE, TRUE, position );
  update_marks( GTK_SCALE( scale9v ), TRUE, GTK_POS_RIGHT );

  scale10v = create_vertical_scale( value, TRUE, TRUE, position );

  scale11v = create_vertical_scale( value, TRUE, TRUE, position );
  update_marks( GTK_SCALE( scale11v ), FALSE, GTK_POS_RIGHT );

  scale12v = create_vertical_scale( value, TRUE, TRUE, position );
  update_marks( GTK_SCALE( scale12v ), TRUE, GTK_POS_RIGHT );

  // horizontal scales
  scale1h = create_horizontal_scale( value, FALSE, FALSE, position );

  scale2h = create_horizontal_scale( value, FALSE, FALSE, position );
  update_marks( GTK_SCALE( scale2h ), FALSE, GTK_POS_TOP );

  scale3h = create_horizontal_scale( value, FALSE, FALSE, position );
  update_marks( GTK_SCALE( scale3h ), TRUE, GTK_POS_TOP );

  scale4h = create_horizontal_scale( value, TRUE, FALSE, position );

  scale5h = create_horizontal_scale( value, TRUE, FALSE, position );
  update_marks( GTK_SCALE( scale5h ), FALSE, GTK_POS_TOP );

  scale6h = create_horizontal_scale( value, TRUE, FALSE, position );
  update_marks( GTK_SCALE( scale6h ), TRUE, GTK_POS_TOP );

  scale7h = create_horizontal_scale( value, FALSE, TRUE, position );

  scale8h = create_horizontal_scale( value, FALSE, TRUE, position );
  update_marks( GTK_SCALE( scale8h ), FALSE, GTK_POS_BOTTOM );

  scale9h = create_horizontal_scale( value, FALSE, TRUE, position );
  update_marks( GTK_SCALE( scale9h ), TRUE, GTK_POS_BOTTOM );

  scale10h = create_horizontal_scale( value, TRUE, TRUE, position );

  scale11h = create_horizontal_scale( value, TRUE, TRUE, position );
  update_marks( GTK_SCALE( scale11h ), FALSE, GTK_POS_BOTTOM );

  scale12h = create_horizontal_scale( value, TRUE, TRUE, position );
  update_marks( GTK_SCALE( scale12h ), TRUE, GTK_POS_BOTTOM );

  // dialog
  area = gtk_dialog_get_content_area( GTK_DIALOG( dialog ) );
  add_to( GTK_BOX( area ), hbox, TRUE, TRUE, 5, 5 );
  add_to( GTK_BOX( hbox ), vbox1, TRUE, TRUE, 5, 5 );
  add_to( GTK_BOX( vbox1 ), scale1h, FALSE, FALSE, 5, 5 );
  add_to( GTK_BOX( vbox1 ), scale2h, FALSE, FALSE, 5, 5 );
  add_to( GTK_BOX( vbox1 ), scale3h, FALSE, FALSE, 5, 5 );
  add_to( GTK_BOX( vbox1 ), scale4h, FALSE, FALSE, 5, 5 );
  add_to( GTK_BOX( vbox1 ), scale5h, FALSE, FALSE, 5, 5 );
  add_to( GTK_BOX( vbox1 ), scale6h, FALSE, FALSE, 5, 5 );
  add_to( GTK_BOX( hbox ), vbox2, TRUE, TRUE, 5, 5 );
  add_to( GTK_BOX( vbox2 ), hboxa, TRUE, TRUE, 0, 0 );
  add_to( GTK_BOX( hboxa ), scale1v, FALSE, FALSE, 5, 5 );
  add_to( GTK_BOX( hboxa ), scale2v, FALSE, FALSE, 5, 5 );
  add_to( GTK_BOX( hboxa ), scale3v, FALSE, FALSE, 5, 5 );
  add_to( GTK_BOX( hboxa ), scale7v, FALSE, FALSE, 5, 5 );
  add_to( GTK_BOX( hboxa ), scale8v, FALSE, FALSE, 5, 5 );
  add_to( GTK_BOX( hboxa ), scale9v, FALSE, FALSE, 5, 5 );
  add_to( GTK_BOX( vbox2 ), hboxb, TRUE, TRUE, 0, 0 );
  add_to( GTK_BOX( hboxb ), scale4v, FALSE, FALSE, 5, 5 );
  add_to( GTK_BOX( hboxb ), scale5v, FALSE, FALSE, 5, 5 );
  add_to( GTK_BOX( hboxb ), scale6v, FALSE, FALSE, 5, 5 );
  add_to( GTK_BOX( hboxb ), scale10v, FALSE, FALSE, 5, 5 );
  add_to( GTK_BOX( hboxb ), scale11v, FALSE, FALSE, 5, 5 );
  add_to( GTK_BOX( hboxb ), scale12v, FALSE, FALSE, 5, 5 );
  add_to( GTK_BOX( hbox ), vbox3, TRUE, TRUE, 5, 5 );
  add_to( GTK_BOX( vbox3 ), scale7h, FALSE, FALSE, 5, 5 );
  add_to( GTK_BOX( vbox3 ), scale8h, FALSE, FALSE, 5, 5 );
  add_to( GTK_BOX( vbox3 ), scale9h, FALSE, FALSE, 5, 5 );
  add_to( GTK_BOX( vbox3 ), scale10h, FALSE, FALSE, 5, 5 );
  add_to( GTK_BOX( vbox3 ), scale11h, FALSE, FALSE, 5, 5 );
  add_to( GTK_BOX( vbox3 ), scale12h, FALSE, FALSE, 5, 5 );

#if GTK_CHECK_VERSION( 4, 0, 0 )
  // https://blog.gtk.org/2022/10/30/on-deprecations/
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_dialog_add_button( GTK_DIALOG( dialog ), _gtk( "_Cancel" ), 0 );
  gtk_dialog_add_button( GTK_DIALOG( dialog ), _gtk( "_OK" ), 0 );
  G_GNUC_END_IGNORE_DEPRECATIONS
#else
  gtk_dialog_add_button( GTK_DIALOG( dialog ), "gtk-cancel", 0 );
  gtk_dialog_add_button( GTK_DIALOG( dialog ), "gtk-ok", 0 );
  gtk_widget_show_all( dialog );
#endif

  gtk_window_set_resizable( GTK_WINDOW( dialog ), FALSE );
  gtk_window_set_title( GTK_WINDOW( dialog ), "GtkDialog" );
  gtk_window_set_icon_name( GTK_WINDOW( dialog ), GETTEXT_PACKAGE );

#if GTK_CHECK_VERSION( 4, 0, 0 )
  gtk_widget_set_visible( dialog, TRUE );
  g_signal_connect( dialog, "response", G_CALLBACK( closedialog ), NULL );
#else
  gtk_dialog_run( GTK_DIALOG( dialog ) );
  gtk_widget_destroy( dialog );
#endif
}

static void dialog_scales_top() {
  dialog_scales( GTK_POS_TOP );
}

static void dialog_scales_bottom() {
  dialog_scales( GTK_POS_BOTTOM );
}

static void dialog_scales_right() {
  dialog_scales( GTK_POS_RIGHT );
}

static void dialog_scales_left() {
  dialog_scales( GTK_POS_LEFT );
}

static gboolean on_scrolltabs( GtkWidget* widget, GdkEventScroll* event ) {
  // gtk-scroll-tabs for GTK 3.4 to 3.24
  GtkWidget *child, *event_widget, *action_widget;
  GtkNotebook* notebook;

  while( !GTK_IS_NOTEBOOK( widget ) )
    widget = gtk_widget_get_parent( widget );

  notebook = GTK_NOTEBOOK( widget );

  child = gtk_notebook_get_nth_page( notebook, gtk_notebook_get_current_page( notebook ) );
  if( child == NULL )
    return FALSE;

  // ignore scroll events from the content of the page
  event_widget = gtk_get_event_widget( (GdkEvent*) event );
  if( event_widget == NULL || event_widget == child || gtk_widget_is_ancestor( event_widget, child ) )
    return FALSE;

  // and also from the action widgets
  action_widget = gtk_notebook_get_action_widget( notebook, GTK_PACK_START );
  if( event_widget == action_widget || ( action_widget != NULL && gtk_widget_is_ancestor( event_widget, action_widget ) ) )
    return FALSE;
  action_widget = gtk_notebook_get_action_widget( notebook, GTK_PACK_END );
  if( event_widget == action_widget || ( action_widget != NULL && gtk_widget_is_ancestor( event_widget, action_widget ) ) )
    return FALSE;

  switch( event->direction ) {
    case GDK_SCROLL_RIGHT:
    case GDK_SCROLL_DOWN:
      gtk_notebook_next_page( notebook );
      break;
    case GDK_SCROLL_LEFT:
    case GDK_SCROLL_UP:
      gtk_notebook_prev_page( notebook );
      break;
    case GDK_SCROLL_SMOOTH:
      switch( gtk_notebook_get_tab_pos( notebook ) ) {
        case GTK_POS_LEFT:
        case GTK_POS_RIGHT:
          if( event->delta_y > 0 )
            gtk_notebook_next_page( notebook );
          else if( event->delta_y < 0 )
            gtk_notebook_prev_page( notebook );
          break;
        case GTK_POS_TOP:
        case GTK_POS_BOTTOM:
          if( event->delta_x > 0 )
            gtk_notebook_next_page( notebook );
          else if( event->delta_x < 0 )
            gtk_notebook_prev_page( notebook );
          break;
      }
      break;
  }

  return TRUE;
}

static void dialog_message() {
  GtkWidget* dialog = gtk_message_dialog_new(
      GTK_WINDOW( window ),
      GTK_DIALOG_DESTROY_WITH_PARENT,
      GTK_MESSAGE_INFO,
      GTK_BUTTONS_YES_NO,
      "GtkMessageDialog" );

  gtk_message_dialog_format_secondary_text( GTK_MESSAGE_DIALOG( dialog ), _app( "A widget factory is a theme preview application for GTK. It displays the various widget types provided by GTK in a single window allowing to see the visual effect of the applied theme." ) );

  gtk_window_set_title( GTK_WINDOW( dialog ), "GtkMessageDialog" );
  gtk_window_set_icon_name( GTK_WINDOW( dialog ), GETTEXT_PACKAGE );

  gtk_dialog_run( GTK_DIALOG( dialog ) );
  gtk_widget_destroy( dialog );
}

static void dialog_page_setup() {
  GtkWidget* dialog = gtk_page_setup_unix_dialog_new( "GtkPageSetupUnixDialog", GTK_WINDOW( window ) );
  gtk_window_set_icon_name( GTK_WINDOW( dialog ), GETTEXT_PACKAGE );

  gtk_dialog_run( GTK_DIALOG( dialog ) );
  gtk_widget_destroy( dialog );
}

static void dialog_save() {
  GtkWidget* dialog = gtk_file_chooser_dialog_new(
      "GtkFileChooserDialog",
      GTK_WINDOW( window ),
      GTK_FILE_CHOOSER_ACTION_SAVE,
      "gtk-cancel",
      GTK_RESPONSE_CANCEL,
      "gtk-save",
      GTK_RESPONSE_ACCEPT,
      NULL );

  gtk_window_set_icon_name( GTK_WINDOW( dialog ), GETTEXT_PACKAGE );
  gtk_dialog_run( GTK_DIALOG( dialog ) );
  gtk_widget_destroy( dialog );
}

static void activate_action( GSimpleAction* action, GVariant* parameter, gpointer data ) {
  GVariant* action_state;

  // radio
  if( parameter ) {
    g_simple_action_set_state( action, parameter );
    if( strcmp( (gchar*) g_action_get_name( G_ACTION( action ) ), "set-theme" ) == 0 )
      update_theme( (gchar*) g_variant_get_string( parameter, NULL ) );
    // else if( strcmp( (gchar*) g_action_get_name( G_ACTION( action ) ), "set-direction" ) == 0 )
    // update_text_direction( ( strcmp( g_variant_get_string( parameter, NULL ), "1" ) == 0 ) ? 1 : 2 );
  }
  // checkbox
  else {
    action_state = g_action_get_state( G_ACTION( action ) );
    if( g_variant_get_boolean( action_state ) == FALSE )
      g_simple_action_set_state( action, g_variant_new_boolean( TRUE ) );
    else
      g_simple_action_set_state( action, g_variant_new_boolean( FALSE ) );
  }
}

static void accels_select( GtkWidget* widget ) {
  // gtk-can-change-accels for GTK 3.x (3.2/3.24)
  current_menuitem = widget;
}

static void accels_deselect( GtkWidget* widget ) {
  // gtk-can-change-accels for GTK 3.x (3.2/3.24)
  if( current_menuitem == widget )
    current_menuitem = NULL;
}

static GtkWidget* create_menuitem( GtkWidget* menu, gchar* text, gboolean disabled, gchar* accel, gchar* keymap, GCallback function ) {
  GtkWidget* menuitem;
  GdkModifierType mods;
  guint key;

  menuitem = gtk_image_menu_item_new_from_stock( text, NULL );
  gtk_menu_shell_append( GTK_MENU_SHELL( menu ), menuitem );
  gtk_menu_item_set_accel_path( GTK_MENU_ITEM( menuitem ), keymap );
  gtk_widget_set_sensitive( menuitem, !disabled );

  if( accel ) {
    gtk_accelerator_parse( accel, &key, &mods );
    gtk_accel_map_add_entry( keymap, key, mods );
  }

  if( function )
    g_signal_connect( menuitem, "activate", function, NULL );

  // gtk-can-change-accels for GTK 3.x (3.2/3.24)
  g_signal_connect( menuitem, "select", G_CALLBACK( accels_select ), NULL );
  g_signal_connect( menuitem, "deselect", G_CALLBACK( accels_deselect ), NULL );

  return menuitem;
}

static GtkWidget* create_menuitem_radio( GtkWidget* menu, gchar* text, gboolean checked, gboolean inconsistent, gboolean disabled, GSList* group ) {
  GtkWidget* menuitem;

  menuitem = gtk_radio_menu_item_new_with_mnemonic( group, text );
  gtk_check_menu_item_set_inconsistent( GTK_CHECK_MENU_ITEM( menuitem ), inconsistent );
  gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( menuitem ), checked );
  gtk_widget_set_sensitive( menuitem, !disabled );
  gtk_menu_shell_append( GTK_MENU_SHELL( menu ), menuitem );

  return menuitem;
}

static void activate( GApplication* app ) {
  GtkBuilder* builder;
  GtkWindow* window;
  GtkWidget* widget;
  GtkWidget* widget2;
  GtkWidget* widget3;
  GtkWidget* widget4;
  GtkWidget* stack;
  GtkWidget* dialog;
  GtkAdjustment* adj;
  GtkCssProvider* provider;
  static GActionEntry win_entries [] = {
      {"dark",        NULL,                NULL, "false", change_theme_state     },
      { "transition", NULL,                NULL, "false", change_transition_state},
      { "search",     activate_search,     NULL, NULL,    NULL                   },
      { "delete",     activate_delete,     NULL, NULL,    NULL                   },
      { "busy",       get_busy,            NULL, NULL,    NULL                   },
      { "background", activate_background, NULL, NULL,    NULL                   },
      { "open",       activate_open,       NULL, NULL,    NULL                   },
      { "record",     activate_record,     NULL, NULL,    NULL                   },
      { "lock",       activate_lock,       NULL, NULL,    NULL                   },
  };

  struct {
      const gchar* action_and_target;
      const gchar* accelerators [ 2 ];
  } accels [] = {
      {"app.about",       { "F1", NULL }               },
      { "app.quit",       { "<Control>Q", NULL }       },
      { "win.dark",       { "<Primary>d", NULL }       },
      { "win.search",     { "<Primary>s", NULL }       },
      { "win.delete",     { "Delete", NULL }           },
      { "win.background", { "<Primary>b", NULL }       },
      { "win.open",       { "<Control>O", NULL }       },
      { "win.record",     { "<Primary>r", NULL }       },
      { "win.lock",       { "<Primary>l", NULL }       },

 // the new stuff, bc idk how to organize :)
      { "win.about",      { "F1", NULL }               },
      { "app.print",      { "<Control>P", NULL }       },
      { "copy",           { "<Control>C", NULL }       },
      { "cut",            { "<Control>X", NULL }       },
      { "paste",          { "<Control>V", NULL }       },
      { "app.close",      { "<Control>W", NULL }       },
      { "app.pagesetup",  { "<Control><Shift>P", NULL }},
      { "app.properties", { "<Alt>Return", NULL }      },
      { "app.refresh",    { "F5", NULL }               },
      { "app.save",       { "<Control>S", NULL }       },
  };

  gint i;
  GPermission* permission;
  GAction* action;

  g_type_ensure( my_text_view_get_type() );
  register_icon_sizes();

  provider = gtk_css_provider_new();
  gtk_css_provider_load_from_resource( provider, "/org/gtk/WidgetFactory/awf.css" );
  gtk_style_context_add_provider_for_screen( gdk_screen_get_default(),
      GTK_STYLE_PROVIDER( provider ),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION );
  g_object_unref( provider );

  update_theme( current_theme );

  builder = gtk_builder_new_from_resource( "/org/gtk/WidgetFactory/awf.ui" );
  gtk_builder_add_callback_symbol( builder, "on_entry_icon_release", (GCallback) on_entry_icon_release );
  gtk_builder_add_callback_symbol( builder, "on_scale_button_value_changed", (GCallback) on_scale_button_value_changed );
  gtk_builder_add_callback_symbol( builder, "on_scale_button_query_tooltip", (GCallback) on_scale_button_query_tooltip );
  gtk_builder_add_callback_symbol( builder, "on_record_button_toggled", (GCallback) on_record_button_toggled );
  gtk_builder_add_callback_symbol( builder, "on_page_combo_changed", (GCallback) on_page_combo_changed );
  gtk_builder_add_callback_symbol( builder, "on_range_from_changed", (GCallback) on_range_from_changed );
  gtk_builder_add_callback_symbol( builder, "on_range_to_changed", (GCallback) on_range_to_changed );
  gtk_builder_add_callback_symbol( builder, "osd_frame_button_press", (GCallback) osd_frame_button_press );
  gtk_builder_add_callback_symbol( builder, "tab_close_cb", (GCallback) tab_close_cb );
  gtk_builder_add_callback_symbol( builder, "increase_icon_size", (GCallback) increase_icon_size );
  gtk_builder_add_callback_symbol( builder, "decrease_icon_size", (GCallback) decrease_icon_size );
  gtk_builder_add_callback_symbol( builder, "reset_icon_size", (GCallback) reset_icon_size );
  gtk_builder_add_callback_symbol( builder, "scale_format_value", (GCallback) scale_format_value );
  gtk_builder_add_callback_symbol( builder, "scale_format_value_blank", (GCallback) scale_format_value_blank );
  gtk_builder_add_callback_symbol( builder, "validate_more_details", (GCallback) validate_more_details );
  gtk_builder_add_callback_symbol( builder, "mode_switch_state_set", (GCallback) mode_switch_state_set );
  gtk_builder_add_callback_symbol( builder, "level_scale_value_changed", (GCallback) level_scale_value_changed );

  gtk_builder_add_callback_symbol( builder, "menubar_open", (GCallback) dialog_open );
  gtk_builder_add_callback_symbol( builder, "menubar_recent", (GCallback) dialog_recent );
  gtk_builder_add_callback_symbol( builder, "menubar_scalestop", (GCallback) dialog_scales_top );
  gtk_builder_add_callback_symbol( builder, "menubar_scalesbottom", (GCallback) dialog_scales_bottom );
  gtk_builder_add_callback_symbol( builder, "menubar_scalesright", (GCallback) dialog_scales_right );
  gtk_builder_add_callback_symbol( builder, "menubar_scalesleft", (GCallback) dialog_scales_top );
  gtk_builder_add_callback_symbol( builder, "menubar_save", (GCallback) dialog_save );
  gtk_builder_add_callback_symbol( builder, "menubar_refresh", (GCallback) update_theme );
  gtk_builder_add_callback_symbol( builder, "menubar_properties", (GCallback) dialog_message );
  gtk_builder_add_callback_symbol( builder, "menubar_pagesetup", (GCallback) dialog_page_setup );
  gtk_builder_add_callback_symbol( builder, "menubar_print", (GCallback) dialog_print );
  // i am lazy :)
  // gtk_builder_add_callback_symbol( builder, "menubar_about", (GCallback) activate_about );

  gtk_builder_connect_signals( builder, NULL );

  window = (GtkWindow*) gtk_builder_get_object( builder, "window" );
  gtk_application_add_window( GTK_APPLICATION( app ), window );
  g_action_map_add_action_entries( G_ACTION_MAP( window ),
      win_entries, G_N_ELEMENTS( win_entries ),
      window );

  for( i = 0; i < G_N_ELEMENTS( accels ); i++ )
    gtk_application_set_accels_for_action( GTK_APPLICATION( app ), accels [ i ].action_and_target, accels [ i ].accelerators );

  widget = (GtkWidget*) gtk_builder_get_object( builder, "statusbar" );
  gtk_statusbar_push( GTK_STATUSBAR( widget ), 0, "All systems are operating normally." );
  action = G_ACTION( g_property_action_new( "statusbar", widget, "visible" ) );
  g_action_map_add_action( G_ACTION_MAP( window ), action );
  g_object_unref( G_OBJECT( action ) );

  widget = (GtkWidget*) gtk_builder_get_object( builder, "toolbar" );
  action = G_ACTION( g_property_action_new( "toolbar", widget, "visible" ) );
  g_action_map_add_action( G_ACTION_MAP( window ), action );
  g_object_unref( G_OBJECT( action ) );

  widget = (GtkWidget*) gtk_builder_get_object( builder, "menubar_system_themes" );
  action = G_ACTION( g_property_action_new( "menubar_system_themes", widget, "visible" ) );
  g_action_map_add_action( G_ACTION_MAP( window ), action );
  g_object_unref( G_OBJECT( action ) );

  widget = (GtkWidget*) gtk_builder_get_object( builder, "menubar1" );
  action = G_ACTION( g_property_action_new( "menubar1", widget, "visible" ) );
  g_action_map_add_action( G_ACTION_MAP( window ), action );
  g_object_unref( G_OBJECT( action ) );

  widget = (GtkWidget*) gtk_builder_get_object( builder, "statusbarlabel" );
  action = G_ACTION( g_property_action_new( "statusbarlabel", widget, "visible" ) );
  g_action_map_add_action( G_ACTION_MAP( window ), action );
  g_object_unref( G_OBJECT( action ) );

  adj = (GtkAdjustment*) gtk_builder_get_object( builder, "adjustment1" );

  widget = (GtkWidget*) gtk_builder_get_object( builder, "progressbar3" );
  g_signal_connect( adj, "value-changed", G_CALLBACK( update_pulse_time ), widget );
  update_pulse_time( adj, widget );

  widget = (GtkWidget*) gtk_builder_get_object( builder, "entry1" );
  g_signal_connect( adj, "value-changed", G_CALLBACK( update_pulse_time ), widget );
  update_pulse_time( adj, widget );

  widget = (GtkWidget*) gtk_builder_get_object( builder, "page2reset" );
  adj = (GtkAdjustment*) gtk_builder_get_object( builder, "adjustment2" );
  g_signal_connect( widget, "clicked", G_CALLBACK( spin_value_reset ), adj );

  widget = (GtkWidget*) gtk_builder_get_object( builder, "page2dismiss" );
  g_signal_connect( widget, "clicked", G_CALLBACK( dismiss ), NULL );

  widget = (GtkWidget*) gtk_builder_get_object( builder, "page2note" );
  adj = (GtkAdjustment*) gtk_builder_get_object( builder, "adjustment2" );
  g_signal_connect( adj, "value-changed", G_CALLBACK( spin_value_changed ), widget );

  widget = (GtkWidget*) gtk_builder_get_object( builder, "listbox" );
  gtk_list_box_set_header_func( GTK_LIST_BOX( widget ), update_header, NULL, NULL );
  g_signal_connect( widget, "row-activated", G_CALLBACK( row_activated ), NULL );

  widget2 = (GtkWidget*) gtk_builder_get_object( builder, "listboxrow1switch" );
  g_signal_connect( widget2, "notify::active", G_CALLBACK( toggle_selection_mode ), widget );

  widget = (GtkWidget*) gtk_builder_get_object( builder, "listboxrow3" );
  widget2 = (GtkWidget*) gtk_builder_get_object( builder, "listboxrow3image" );
  g_object_set_data( G_OBJECT( widget ), "image", widget2 );

  widget = (GtkWidget*) gtk_builder_get_object( builder, "listboxrow4" );
  widget2 = (GtkWidget*) gtk_builder_get_object( builder, "info_dialog" );
  g_object_set_data( G_OBJECT( widget ), "dialog", widget2 );

  widget = (GtkWidget*) gtk_builder_get_object( builder, "listboxrow5button" );
  widget2 = (GtkWidget*) gtk_builder_get_object( builder, "action_dialog" );
  g_signal_connect_swapped( widget, "clicked", G_CALLBACK( gtk_window_present ), widget2 );

  widget = (GtkWidget*) gtk_builder_get_object( builder, "toolbar" );
  g_object_set_data( G_OBJECT( window ), "toolbar", widget );

  widget = (GtkWidget*) gtk_builder_get_object( builder, "searchbar" );
  g_object_set_data( G_OBJECT( window ), "searchbar", widget );

  widget = (GtkWidget*) gtk_builder_get_object( builder, "infobar" );
  g_signal_connect( widget, "response", G_CALLBACK( info_bar_response ), NULL );
  g_object_set_data( G_OBJECT( window ), "infobar", widget );

  dialog = (GtkWidget*) gtk_builder_get_object( builder, "info_dialog" );
  g_signal_connect( dialog, "response", G_CALLBACK( close_dialog ), NULL );
  widget = (GtkWidget*) gtk_builder_get_object( builder, "info_dialog_button" );
  g_signal_connect( widget, "clicked", G_CALLBACK( show_dialog ), dialog );

  dialog = (GtkWidget*) gtk_builder_get_object( builder, "action_dialog" );
  g_signal_connect( dialog, "response", G_CALLBACK( close_dialog ), NULL );
  widget = (GtkWidget*) gtk_builder_get_object( builder, "action_dialog_button" );
  g_signal_connect( widget, "clicked", G_CALLBACK( show_dialog ), dialog );

  widget = (GtkWidget*) gtk_builder_get_object( builder, "act_action_dialog" );
  stack = (GtkWidget*) gtk_builder_get_object( builder, "toplevel_stack" );
  g_signal_connect( widget, "clicked", G_CALLBACK( action_dialog_button_clicked ), stack );
  g_signal_connect( stack, "notify::visible-child-name", G_CALLBACK( page_changed_cb ), NULL );
  page_changed_cb( stack, NULL, NULL );

  page_stack = stack;

  dialog = (GtkWidget*) gtk_builder_get_object( builder, "preference_dialog" );
  g_signal_connect( dialog, "response", G_CALLBACK( close_dialog ), NULL );
  widget = (GtkWidget*) gtk_builder_get_object( builder, "preference_dialog_button" );
  g_signal_connect( widget, "clicked", G_CALLBACK( show_dialog ), dialog );
  widget = (GtkWidget*) gtk_builder_get_object( builder, "circular_button" );
  g_signal_connect( widget, "clicked", G_CALLBACK( show_dialog ), dialog );

  widget = (GtkWidget*) gtk_builder_get_object( builder, "level_scale" );
  g_object_set_data( G_OBJECT( dialog ), "level_scale", widget );
  widget = (GtkWidget*) gtk_builder_get_object( builder, "mode_switch" );
  g_object_set_data( G_OBJECT( dialog ), "mode_switch", widget );
  widget = (GtkWidget*) gtk_builder_get_object( builder, "error_label" );
  g_object_set_data( G_OBJECT( dialog ), "error_label", widget );

  dialog = (GtkWidget*) gtk_builder_get_object( builder, "selection_dialog" );
  g_object_set_data( G_OBJECT( window ), "selection_dialog", dialog );
  widget = (GtkWidget*) gtk_builder_get_object( builder, "text3" );
  g_signal_connect( dialog, "response", G_CALLBACK( close_selection_dialog ), widget );
  widget = (GtkWidget*) gtk_builder_get_object( builder, "selection_dialog_button" );
  g_signal_connect( widget, "clicked", G_CALLBACK( show_dialog ), dialog );

  widget2 = (GtkWidget*) gtk_builder_get_object( builder, "selection_flowbox" );
  g_object_set_data( G_OBJECT( window ), "selection_flowbox", widget2 );
  g_signal_connect_swapped( widget, "clicked", G_CALLBACK( populate_flowbox ), widget2 );

  widget = (GtkWidget*) gtk_builder_get_object( builder, "charletree" );
  populate_model( (GtkTreeStore*) gtk_tree_view_get_model( GTK_TREE_VIEW( widget ) ) );
  gtk_tree_view_set_row_separator_func( GTK_TREE_VIEW( widget ), row_separator_func, NULL, NULL );
  gtk_tree_view_expand_all( GTK_TREE_VIEW( widget ) );

  widget = GTK_WIDGET( gtk_builder_get_object( builder, "munsell" ) );
  widget2 = GTK_WIDGET( gtk_builder_get_object( builder, "cchooser" ) );

  populate_colors( widget, widget2 );
  g_signal_connect( widget2, "notify::rgba", G_CALLBACK( rgba_changed ), widget );

  widget = (GtkWidget*) gtk_builder_get_object( builder, "page_combo" );
  gtk_combo_box_set_row_separator_func( GTK_COMBO_BOX( widget ), page_combo_separator_func, NULL, NULL );
  widget2 = (GtkWidget*) gtk_builder_get_object( builder, "range_from_spin" );
  widget3 = (GtkWidget*) gtk_builder_get_object( builder, "range_to_spin" );
  widget4 = (GtkWidget*) gtk_builder_get_object( builder, "print_button" );
  g_object_set_data( G_OBJECT( widget ), "range_from_spin", widget2 );
  g_object_set_data( G_OBJECT( widget3 ), "range_from_spin", widget2 );
  g_object_set_data( G_OBJECT( widget ), "range_to_spin", widget3 );
  g_object_set_data( G_OBJECT( widget2 ), "range_to_spin", widget3 );
  g_object_set_data( G_OBJECT( widget ), "print_button", widget4 );

  set_accel( GTK_APPLICATION( app ), GTK_WIDGET( gtk_builder_get_object( builder, "quitmenuitem" ) ) );
  set_accel( GTK_APPLICATION( app ), GTK_WIDGET( gtk_builder_get_object( builder, "deletemenuitem" ) ) );
  set_accel( GTK_APPLICATION( app ), GTK_WIDGET( gtk_builder_get_object( builder, "searchmenuitem" ) ) );
  set_accel( GTK_APPLICATION( app ), GTK_WIDGET( gtk_builder_get_object( builder, "darkmenuitem" ) ) );
  set_accel( GTK_APPLICATION( app ), GTK_WIDGET( gtk_builder_get_object( builder, "aboutmenuitem" ) ) );
  set_accel( GTK_APPLICATION( app ), GTK_WIDGET( gtk_builder_get_object( builder, "bgmenuitem" ) ) );

  set_accel( GTK_APPLICATION( app ), GTK_WIDGET( gtk_builder_get_object( builder, "menubar_open" ) ) );
  set_accel( GTK_APPLICATION( app ), GTK_WIDGET( gtk_builder_get_object( builder, "menubar_save" ) ) );
  set_accel( GTK_APPLICATION( app ), GTK_WIDGET( gtk_builder_get_object( builder, "menubar_refresh" ) ) );
  set_accel( GTK_APPLICATION( app ), GTK_WIDGET( gtk_builder_get_object( builder, "menubar_properties" ) ) );
  set_accel( GTK_APPLICATION( app ), GTK_WIDGET( gtk_builder_get_object( builder, "menubar_pagesetup" ) ) );
  set_accel( GTK_APPLICATION( app ), GTK_WIDGET( gtk_builder_get_object( builder, "menubar_about" ) ) );
  set_accel( GTK_APPLICATION( app ), GTK_WIDGET( gtk_builder_get_object( builder, "menubar_print" ) ) );
  set_accel( GTK_APPLICATION( app ), GTK_WIDGET( gtk_builder_get_object( builder, "menubar_more_cut" ) ) );
  set_accel( GTK_APPLICATION( app ), GTK_WIDGET( gtk_builder_get_object( builder, "menubar_more_copy" ) ) );
  set_accel( GTK_APPLICATION( app ), GTK_WIDGET( gtk_builder_get_object( builder, "menubar_more_paste" ) ) );
  set_accel( GTK_APPLICATION( app ), GTK_WIDGET( gtk_builder_get_object( builder, "menubar_close" ) ) );
  set_accel( GTK_APPLICATION( app ), GTK_WIDGET( gtk_builder_get_object( builder, "menubar_quit" ) ) );
  set_accel( GTK_APPLICATION( app ), GTK_WIDGET( gtk_builder_get_object( builder, "menubar_pagesetup" ) ) );

  widget2 = (GtkWidget*) gtk_builder_get_object( builder, "tooltextview" );

  widget = (GtkWidget*) gtk_builder_get_object( builder, "toolbutton1" );
  g_signal_connect( widget, "clicked", G_CALLBACK( handle_insert ), widget2 );
  widget = (GtkWidget*) gtk_builder_get_object( builder, "toolbutton2" );
  g_signal_connect( widget, "clicked", G_CALLBACK( handle_insert ), widget2 );
  widget = (GtkWidget*) gtk_builder_get_object( builder, "toolbutton3" );
  g_signal_connect( widget, "clicked", G_CALLBACK( handle_insert ), widget2 );
  widget = (GtkWidget*) gtk_builder_get_object( builder, "toolbutton4" );
  g_signal_connect( widget, "clicked", G_CALLBACK( handle_insert ), widget2 );
  widget = (GtkWidget*) gtk_builder_get_object( builder, "cutbutton" );
  g_signal_connect( widget, "clicked", G_CALLBACK( handle_cutcopypaste ), widget2 );
  g_signal_connect( gtk_text_view_get_buffer( GTK_TEXT_VIEW( widget2 ) ), "notify::has-selection",
      G_CALLBACK( textbuffer_notify_selection ), widget );
  widget = (GtkWidget*) gtk_builder_get_object( builder, "copybutton" );
  g_signal_connect( widget, "clicked", G_CALLBACK( handle_cutcopypaste ), widget2 );
  g_signal_connect( gtk_text_view_get_buffer( GTK_TEXT_VIEW( widget2 ) ), "notify::has-selection",
      G_CALLBACK( textbuffer_notify_selection ), widget );
  widget = (GtkWidget*) gtk_builder_get_object( builder, "deletebutton" );
  g_signal_connect( widget, "clicked", G_CALLBACK( handle_cutcopypaste ), widget2 );
  g_signal_connect( gtk_text_view_get_buffer( GTK_TEXT_VIEW( widget2 ) ), "notify::has-selection",
      G_CALLBACK( textbuffer_notify_selection ), widget );
  widget = (GtkWidget*) gtk_builder_get_object( builder, "pastebutton" );
  g_signal_connect( widget, "clicked", G_CALLBACK( handle_cutcopypaste ), widget2 );
  g_signal_connect_object( gtk_widget_get_clipboard( widget2, GDK_SELECTION_CLIPBOARD ), "owner-change",
      G_CALLBACK( clipboard_owner_change ), widget, 0 );

  widget = (GtkWidget*) gtk_builder_get_object( builder, "osd_frame" );
  widget2 = (GtkWidget*) gtk_builder_get_object( builder, "totem_like_osd" );
  g_object_set_data( G_OBJECT( widget ), "osd", widget2 );

  widget = (GtkWidget*) gtk_builder_get_object( builder, "textview1" );
  g_signal_connect( widget, "populate-popup",
      G_CALLBACK( populate_popup ), NULL );

  widget = (GtkWidget*) gtk_builder_get_object( builder, "open_popover" );
  widget2 = (GtkWidget*) gtk_builder_get_object( builder, "open_popover_entry" );
  widget3 = (GtkWidget*) gtk_builder_get_object( builder, "open_popover_button" );
  gtk_popover_set_default_widget( GTK_POPOVER( widget ), widget3 );
  g_signal_connect( widget2, "notify::text", G_CALLBACK( open_popover_text_changed ), widget3 );
  g_signal_connect_swapped( widget3, "clicked", G_CALLBACK( gtk_widget_hide ), widget );
  widget = (GtkWidget*) gtk_builder_get_object( builder, "open_menubutton" );
  g_object_set_data( G_OBJECT( window ), "open_menubutton", widget );
  widget = (GtkWidget*) gtk_builder_get_object( builder, "record_button" );
  g_object_set_data( G_OBJECT( window ), "record_button", widget );

  widget = (GtkWidget*) gtk_builder_get_object( builder, "lockbox" );
  widget2 = (GtkWidget*) gtk_builder_get_object( builder, "lockbutton" );
  g_object_set_data( G_OBJECT( window ), "lockbutton", widget2 );
  permission = g_object_new( g_test_permission_get_type(), NULL );
  g_object_bind_property( permission, "allowed",
      widget, "sensitive",
      G_BINDING_SYNC_CREATE );
  action = g_action_map_lookup_action( G_ACTION_MAP( window ), "open" );
  g_object_bind_property( permission, "allowed",
      action, "enabled",
      G_BINDING_SYNC_CREATE );
  action = g_action_map_lookup_action( G_ACTION_MAP( window ), "record" );
  g_object_bind_property( permission, "allowed",
      action, "enabled",
      G_BINDING_SYNC_CREATE );
  gtk_lock_button_set_permission( GTK_LOCK_BUTTON( widget2 ), permission );
  g_object_unref( permission );

  widget = (GtkWidget*) gtk_builder_get_object( builder, "iconview1" );
  widget2 = (GtkWidget*) gtk_builder_get_object( builder, "increase_button" );
  g_object_set_data( G_OBJECT( widget ), "increase_button", widget2 );
  widget2 = (GtkWidget*) gtk_builder_get_object( builder, "decrease_button" );
  g_object_set_data( G_OBJECT( widget ), "decrease_button", widget2 );

  adj = (GtkAdjustment*) gtk_builder_get_object( builder, "adjustment3" );
  widget = (GtkWidget*) gtk_builder_get_object( builder, "progressbar1" );
  widget2 = (GtkWidget*) gtk_builder_get_object( builder, "progressbar2" );
  g_signal_connect( adj, "value-changed", G_CALLBACK( adjustment3_value_changed ), widget );
  g_signal_connect( adj, "value-changed", G_CALLBACK( adjustment3_value_changed ), widget2 );

  GSList *iterator, *group = NULL;
  GtkWidget* menuitem;

  // themes
  action = g_simple_action_new_stateful( "set-theme", G_VARIANT_TYPE_STRING, g_variant_new_string( current_theme ) );
  g_signal_connect( action, "activate", G_CALLBACK( activate_action ), NULL );
  g_action_map_add_action( G_ACTION_MAP( g_application_get_default() ), G_ACTION( action ) );

  group = NULL;
  GtkWidget* menu = (GtkWidget*) gtk_builder_get_object( builder, "menubar_system_themes_menu" );
  for( iterator = list_system_theme; iterator; iterator = iterator->next ) {
    if( g_slist_find_custom( list_user_theme, iterator->data, &awf_compare_theme ) ) {
      menuitem = create_menuitem_radio( menu, iterator->data, FALSE, FALSE, TRUE, group );
      group = gtk_radio_menu_item_get_group( GTK_RADIO_MENU_ITEM( menuitem ) );
      if( strcmp( (gchar*) current_theme, (gchar*) iterator->data ) == 0 )
        gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( menuitem ), TRUE );
      g_signal_connect_swapped( menuitem, "activate", G_CALLBACK( update_theme ), iterator->data );
    } else {
      menuitem = create_menuitem_radio( menu, iterator->data, FALSE, FALSE, FALSE, group );
      group = gtk_radio_menu_item_get_group( GTK_RADIO_MENU_ITEM( menuitem ) );
      if( strcmp( (gchar*) current_theme, (gchar*) iterator->data ) == 0 )
        gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( menuitem ), TRUE );
      g_signal_connect_swapped( menuitem, "activate", G_CALLBACK( update_theme ), iterator->data );
    }
  }

  if( !list_system_theme )
    create_menuitem( menu, _app( "No themes found" ), TRUE, NULL, NULL, NULL );

  // user themes
  menu = (GtkWidget*) gtk_builder_get_object( builder, "menubar_user_themes_menu" );
  for( iterator = list_user_theme; iterator; iterator = iterator->next ) {
    menuitem = create_menuitem_radio( menu, iterator->data, FALSE, FALSE, FALSE, group );
    group = gtk_radio_menu_item_get_group( GTK_RADIO_MENU_ITEM( menuitem ) );
    if( strcmp( (gchar*) current_theme, (gchar*) iterator->data ) == 0 )
      gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( menuitem ), TRUE );
    g_signal_connect_swapped( menuitem, "activate", G_CALLBACK( update_theme ), iterator->data );
  }

  if( !list_user_theme )
    create_menuitem( menu, _app( "No themes found" ), TRUE, NULL, NULL, NULL );

  // // user themes
  // menu = (GMenu*) gtk_builder_get_object( builder, "menubar_user_themes_menu" );
  // g_menu_append_submenu( root, _app( "_User themes" ), G_MENU_MODEL( menu ) );
  // for( iterator = list_user_theme; iterator; iterator = iterator->next )
  //   g_menu_append_item( menu, g_menu_item_new( iterator->data, g_strdup_printf( "app.set-theme::%s", (gchar*) iterator->data ) ) );

  // if( !list_user_theme )
  //   g_menu_append( menu, _app( "No themes found" ), "disabled" ); // @todo

  gtk_widget_show_all( GTK_WIDGET( window ) );

  g_object_unref( builder );
}

static void
    print_version( void ) {
  g_print( "gtk3-widget-factory %d.%d.%d\n",
      gtk_get_major_version(),
      gtk_get_minor_version(),
      gtk_get_micro_version() );
}

static int
    local_options( GApplication* app,
        GVariantDict* options,
        gpointer data ) {
  gboolean version = FALSE;

  g_variant_dict_lookup( options, "version", "b", &version );

  if( version ) {
    print_version();
    return 0;
  }

  return -1;
}

int main( int argc, char* argv [] ) {
  list_system_theme = awf_load_theme( "/usr/share/themes" );
  list_system_theme = g_slist_sort( list_system_theme, (GCompareFunc) awf_compare_theme );

  gchar* directory = g_build_path( "/", g_getenv( "HOME" ), ".themes", NULL );
  list_user_theme = awf_load_theme( directory );
  list_user_theme = g_slist_sort( list_user_theme, (GCompareFunc) awf_compare_theme );
  g_free( directory );

  GtkApplication* app;
  GAction* action;
  static GActionEntry app_entries [] = {
      {"about",      activate_about,     NULL, NULL,      NULL},
      { "quit",      activate_quit,      NULL, NULL,      NULL},
      { "inspector", activate_inspector, NULL, NULL,      NULL},
      { "main",      NULL,               "s",  "'steak'", NULL},
      { "wine",      NULL,               NULL, "false",   NULL},
      { "beer",      NULL,               NULL, "false",   NULL},
      { "water",     NULL,               NULL, "true",    NULL},
      { "dessert",   NULL,               "s",  "'bars'",  NULL},
      { "pay",       NULL,               "s",  NULL,      NULL}
  };
  gint status;

  app = gtk_application_new( "org.gtk.WidgetFactory", G_APPLICATION_NON_UNIQUE );

  g_action_map_add_action_entries( G_ACTION_MAP( app ),
      app_entries, G_N_ELEMENTS( app_entries ),
      app );
  action = g_action_map_lookup_action( G_ACTION_MAP( app ), "wine" );
  g_simple_action_set_enabled( G_SIMPLE_ACTION( action ), FALSE );

  g_signal_connect( app, "activate", G_CALLBACK( activate ), NULL );

  g_application_add_main_option( G_APPLICATION( app ), "version", 0, 0, G_OPTION_ARG_NONE, "Show program version", NULL );

  g_signal_connect( app, "handle-local-options", G_CALLBACK( local_options ), NULL );
  status = g_application_run( G_APPLICATION( app ), argc, argv );
  g_object_unref( app );

  g_unix_signal_add( SIGHUP, on_sighup, NULL ); // glib >= 2.30

  return status;
}
