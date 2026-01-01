# components/cal_store/CMakeLists.txt

idf_component_register(
  SRCS "src/cal_store.c"
  INCLUDE_DIRS "include"
  REQUIRES nvs_flash
)