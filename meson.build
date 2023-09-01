project('awf', 'c',
  version : '0.1',
  default_options : ['warning_level=3'])

gnome = import('gnome')
gtk_datadir = 'src/'
libgtk_dep = dependency('gtk+-3.0')
gtk_prefix = get_option('prefix')
gtk_applicationsdir = join_paths(gtk_datadir, 'applications')

widgetfactory_sources = [
  'src/awf.c'
]

add_global_arguments('-DVERSION="2.7.0"', language: 'c')



widgetfactory_resources = gnome.compile_resources(
  'awf_resources',
  'awf.gresource.xml',
  source_dir: 'src'
)

foreach s: [ '16', '22', '24', '32', '48', '256']
  icon_destdir = join_paths(gtk_datadir, 'icons', 'hicolor', '@0@x@0@'.format(s), 'apps')
  icons = [join_paths('src/data', '@0@x@0@'.format(s), 'gtk3-widget-factory.png'),
           join_paths('src/data', '@0@x@0@'.format(s), 'gtk3-widget-factory-symbolic.symbolic.png')]
  install_data(icons, install_dir: icon_destdir)
endforeach

widget_factory = executable(
  'awf',
  widgetfactory_sources,
  widgetfactory_resources,
  dependencies: libgtk_dep,
  include_directories : ['/usr/include/gtk-3.0/unix-print'],
  install: true
)

# desktop file
install_data('awf.desktop', install_dir: gtk_applicationsdir)
