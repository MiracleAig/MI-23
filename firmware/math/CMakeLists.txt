add_library(expression_lib STATIC
        expression.cpp
        expression.h
)

target_link_libraries(expression_lib PUBLIC
        firmware_headers
)
