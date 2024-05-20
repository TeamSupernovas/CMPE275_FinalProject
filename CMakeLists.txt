cmake_minimum_required(VERSION 3.5)
project(NetworkCommunication)

# Find the Qt6 package
find_package(Qt6 COMPONENTS Network REQUIRED)

# Find the OpenMP package
find_package(OpenMP REQUIRED)

# Find the MPI package
# find_package(MPI REQUIRED)
# include_directories(SYSTEM ${MPI_INCLUDE_PATH})

# Generate MOC files for server.h
qt6_wrap_cpp(MOC_FILES register.h metadataAnalytics.h )

# Add the executable for the server
add_executable(metadataAnalytics metadataAnalytics.cpp main.cpp ${MOC_FILES})
target_link_libraries(metadataAnalytics Qt6::Network)

# Add the executable for the client
add_executable(analytics analytics.cpp)
target_link_libraries(analytics Qt6::Network)

# Add the executable for the client
add_executable(analytics-1 analytics_1.cpp)
target_link_libraries(analytics-1 Qt6::Network)

# Add the executable for the client
add_executable(analytics-2 analytics_2.cpp)
target_link_libraries(analytics-2 Qt6::Network)

# Add the executable for the client
add_executable(replica_analytics replica_analytics.cpp)
target_link_libraries(replica_analytics Qt6::Network)


# Add the executable for analytics_3
add_executable(analytics-3 analytics_3.cpp)
target_link_libraries(analytics-3 Qt6::Network OpenMP::OpenMP_CXX)
# Add the executable for the client
add_executable(ingestion ingestion.cpp)
target_link_libraries(ingestion Qt6::Network)

# Add the include directory for register.h
target_include_directories(metadataAnalytics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
