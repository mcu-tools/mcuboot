- Zephyr signature and encryption key file path handling has now
  been aligned with Zephyr, this means values can be specified in
  multiple .conf file and the one that last set it will be the set
  value. This also means that key files will no longer be found
  relative to the .conf file and will instead be found relative
  to the build system ``APPLICATION_CONFIG_DIR`` variable, though
  the key file strings are now configured which allows for using
  escaped CMake variables to locate the files, for example with
  ``\${CMAKE_CURRENT_LIST_DIR}`` to specify a file relative to
  the folder that the file is in.
