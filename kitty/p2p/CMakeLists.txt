project(Kitty-P2P)

add_library(kitty-p2p STATIC
        p2p.cpp p2p.h
        pj/dns_resolv.cpp pj/dns_resolv.h
        pj/ice_trans.cpp pj/ice_trans.h
        pj/io_queue.cpp pj/io_queue.h
        pj/nath.cpp pj/nath.h
        pj/pool.cpp pj/pool.h
        pj/timer_heap.cpp pj/timer_heap.h
        uuid.h p2p_stream.h p2p_stream.cpp data_types.h data_types.cpp)

target_link_libraries(kitty-p2p
        kitty-server kitty-log
        ${PJSIP_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT})


add_executable(kitty-p2p-bootstrap bootstrap/bootstrap.cpp uuid.h bootstrap/quest.h bootstrap/quest.cpp data_types.h)
target_link_libraries(kitty-p2p-bootstrap
        ${KITTY_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT})