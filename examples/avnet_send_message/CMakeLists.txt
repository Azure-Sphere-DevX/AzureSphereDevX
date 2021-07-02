#  Copyright (c) Microsoft Corporation. All rights reserved.
#  Licensed under the MIT License.

###################################################################################################################

# Select your developer board by removing the # tag from the beginning of the line

# The default board selected is the AVNET Azure Sphere Starter Kit Revision 1.

# If you are NOT using the AVNET Revision 1 board be sure to comment out the AVNET board

set(AVNET TRUE "AVNET Azure Sphere Starter Kit Revision 1 ")    
# set(AVNET_REV_2 TRUE "AVNET Azure Sphere Starter Kit Revision 2 ")             
# set(SEEED_STUDIO_RDB TRUE "Seeed Studio Azure Sphere MT3620 Development Kit (aka Reference Design Board or rdb)")
# set(SEEED_STUDIO_MINI TRUE "Seeed Studio Azure Sphere MT3620 Mini Dev Board")

###################################################################################################################

cmake_minimum_required (VERSION 3.10)

project (avnet_send_message_example C)

azsphere_configure_tools(TOOLS_REVISION "21.01")
azsphere_configure_api(TARGET_API_SET "9")

add_subdirectory("../../" out)

# Create executable
add_executable (${PROJECT_NAME} main.c)
target_link_libraries (${PROJECT_NAME} applibs pthread gcc_s c azure_sphere_devx)
target_include_directories(${PROJECT_NAME} PUBLIC ../../include )


if(AVNET)
    add_definitions( -DOEM_AVNET=TRUE )
    azsphere_target_hardware_definition(${PROJECT_NAME} TARGET_DIRECTORY "../HardwareDefinitions/avnet_mt3620_sk" TARGET_DEFINITION "azure_sphere_learning_path.json")
endif(AVNET)

if(AVNET_REV_2)
    add_definitions( -DOEM_AVNET=TRUE )
    azsphere_target_hardware_definition(${PROJECT_NAME} TARGET_DIRECTORY "../HardwareDefinitions/avnet_mt3620_sk_rev2" TARGET_DEFINITION "azure_sphere_learning_path.json")
endif(AVNET_REV_2)

if (SEEED_STUDIO_RDB)
    add_definitions( -DOEM_SEEED_STUDIO=TRUE )
    azsphere_target_hardware_definition(${PROJECT_NAME} TARGET_DIRECTORY "../HardwareDefinitions/mt3620_rdb" TARGET_DEFINITION "azure_sphere_learning_path.json")
endif()

if (SEEED_STUDIO_MINI)
    add_definitions( -DOEM_SEEED_STUDIO_MINI=TRUE )
    azsphere_target_hardware_definition(${PROJECT_NAME} TARGET_DIRECTORY "../HardwareDefinitions/seeed_mt3620_mdb" TARGET_DEFINITION "azure_sphere_learning_path.json")
endif(SEEED_STUDIO_MINI)


azsphere_target_add_image_package(${PROJECT_NAME})