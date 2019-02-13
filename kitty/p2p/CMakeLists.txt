project(Kitty-P2P)

add_library(kitty-p2p STATIC
        pack.cpp pack.h p2p.cpp p2p.h
        pj/dns_resolv.cpp pj/dns_resolv.h
        pj/ice_trans.cpp pj/ice_trans.h
        pj/io_queue.cpp pj/io_queue.h
        pj/nath.cpp pj/nath.h
        pj/pool.cpp pj/pool.h
        pj/timer_heap.cpp pj/timer_heap.h
        uuid.h p2p_stream.h p2p_stream.cpp)

target_link_libraries(kitty-p2p
        kitty-server kitty-log
        nlohmann_json::nlohmann_json
        ${PJSIP_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT})


add_executable(kitty-pj-server server/server.cpp uuid.h server/quest.h server/quest.cpp)
target_link_libraries(kitty-pj-server
        nlohmann_json::nlohmann_json
        ${KITTY_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT})