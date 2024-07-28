project('App', 'c', 'cpp', default_options : ['c_std=c11', 'cpp_std=c++20'])

program_src_root = 'src/'

cef_root = './cef'

cef_lib_root = cef_root + '/Release'
cef_include_root = cef_root + '/'

cpp = meson.get_compiler('cpp')

#https://magpcss.org/ceforum/viewtopic.php?f=6&t=18745
add_global_arguments('-DNDEBUG', language: ['c', 'cpp'])

libcef_dll_wrapper = cpp.find_library('cef_dll_wrapper', dirs : meson.current_source_dir() + '/cef/libcef_dll_wrapper')
libcef = cpp.find_library('cef', dirs : meson.current_source_dir() + '/cef/Release')

objs = [
	
]

executable_src = [
	program_src_root + 'whereami.c',
	program_src_root + 'interface.cpp',
	program_src_root + 'tab.cpp',
	program_src_root + 'navbar_interface.cpp',
	program_src_root + 'app.cpp',
	program_src_root + 'handler.cpp',
	program_src_root + 'util.cpp',
	program_src_root + 'handler_sdl.cpp',
	program_src_root + 'logs.cpp',
	program_src_root + 'render_handler.cpp',
	program_src_root + 'fmt/format.cc',
	program_src_root + 'fmt/os.cc',
]

executable_include = [
	program_src_root,
  cef_include_root
]

executable_deps = [
	libcef,
	libcef_dll_wrapper,
	dependency('SDL2'),
]


#Generate executable
shared_library(
	'wlxcefwrapper', 
	sources: executable_src, 
	include_directories: executable_include,
	dependencies: executable_deps,
	link_args: [],
	objects: objs)
