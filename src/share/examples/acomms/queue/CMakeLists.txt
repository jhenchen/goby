get_filename_component(queue_examples_dir ./ ABSOLUTE)
add_definitions(-DQUEUE_EXAMPLES_DIR="${queue_examples_dir}")

add_subdirectory(queue_simple)
add_subdirectory(multimessage)

# broken
#add_subdirectory(encode_on_demand)
