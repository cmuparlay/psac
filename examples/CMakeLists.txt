add_executable(mapreduce mapreduce.cpp)
target_link_libraries(mapreduce PRIVATE psac)
target_compile_options(mapreduce PRIVATE
  -Wall -Wextra -Wfatal-errors
  -g -Og -fno-omit-frame-pointer
  -fsanitize=address,undefined
)
target_link_options(mapreduce PRIVATE
  -fno-omit-frame-pointer
  -fsanitize=address,undefined
)

add_executable(raytracer raytracer.cpp)
target_link_libraries(raytracer PRIVATE psac)
target_compile_options(raytracer PRIVATE
  -Wall -Wextra -Wfatal-errors
)

