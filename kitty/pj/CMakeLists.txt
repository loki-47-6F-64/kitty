project(Kitty-P2P)

add_library(kitty-p2p STATIC
        p2p/pack.cpp p2p/pack.h p2p/quest.cpp p2p/quest.h
        dns_resolv.cpp dns_resolv.h
        ice_trans.cpp ice_trans.h
        io_queue.cpp io_queue.h
        nath.cpp nath.h
        pool.cpp pool.h
        timer_heap.cpp timer_heap.h
        uuid.h)

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