#include "register_types.h"
#include "grpc_client.h"
#include "util/status_map.h"

#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

void initialize_godot_grpc_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

    godot_grpc::Logger::info("Initializing godot_grpc extension");

    ClassDB::register_class<godot_grpc::GrpcClient>();

    godot_grpc::Logger::info("godot_grpc extension initialized");
}

void uninitialize_godot_grpc_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

    godot_grpc::Logger::info("Uninitializing godot_grpc extension");
}

extern "C" {
    // Initialization entry point
    GDExtensionBool GDE_EXPORT godot_grpc_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
        godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

        init_obj.register_initializer(initialize_godot_grpc_module);
        init_obj.register_terminator(uninitialize_godot_grpc_module);
        init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

        return init_obj.init();
    }
}
