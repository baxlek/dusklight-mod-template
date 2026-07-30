#include "mods/hook.hpp"
#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"

#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "d/d_msg_object.h"
#include "f_op/f_op_msg.h"

#include <chrono>
#include <cstring>
#include <ctime>

DEFINE_MOD();

IMPORT_SERVICE(LogService, svc_log);
IMPORT_SERVICE(HookService, svc_hook);

DEFINE_HOOK(&dScnKy_env_light_c::setDaytime, SetDaytime);

static bool is_time_sync_stage(const char* stage_name) {
    return stage_name != nullptr &&
           (!std::strcmp(stage_name, "F_SP00") || !std::strcmp(stage_name, "F_SP103") ||
               !std::strcmp(stage_name, "F_SP104") || !std::strcmp(stage_name, "F_SP109") ||
               !std::strcmp(stage_name, "F_SP111") || !std::strcmp(stage_name, "F_SP118") ||
               !std::strcmp(stage_name, "F_SP128"));
}

static bool should_sync_time(dScnKy_env_light_c* env_light) {
    if (dKy_darkworld_check() || dComIfGp_event_runCheck()) {
        return false;
    }

    msg_class* msg = dMsgObject_c::getActor();
    const bool message_active = msg != nullptr && msg->mode >= 2;
    const bool normal_time_progresses =
        dComIfGp_roomControl_getTimePass() && !env_light->field_0x130a && !message_active;
    return normal_time_progresses || is_time_sync_stage(dComIfGp_getStartStageName());
}

static void on_set_daytime_post(ModContext*, void* args, void*, void*) {
    dScnKy_env_light_c* env_light = mods::arg<dScnKy_env_light_c*>(args, 0);
    if (env_light == nullptr || !should_sync_time(env_light)) {
        return;
    }

    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_time {};
#if defined(_WIN32)
    localtime_s(&local_time, &now_time);
#else
    localtime_r(&now_time, &local_time);
#endif

    const f32 calendar_daytime = local_time.tm_hour * 15.0f +
                                 local_time.tm_min * (15.0f / 60.0f) +
                                 local_time.tm_sec * (15.0f / 3600.0f);

    f32 diff_daytime = calendar_daytime - env_light->daytime;
    if (diff_daytime < 0.0f) {
        diff_daytime += 360.0f;
    }

    if (diff_daytime <= 1.0f) {
        env_light->daytime = calendar_daytime;
    } else {
        env_light->daytime += 1.0f;
    }

    dComIfGs_setTime(env_light->daytime);
}

extern "C" {
MOD_EXPORT ModResult mod_initialize(ModError*) {
    ModResult result = mods::hook_add_post<SetDaytime>(svc_hook, on_set_daytime_post);
    if (result != MOD_OK) {
        svc_log->error(mod_ctx, "failed to install on_set_daytime_post");
        return result;
    }

    svc_log->info(mod_ctx, "time_sync_neo initialized");
    return MOD_OK;
}

MOD_EXPORT ModResult mod_update(ModError*) {
    return MOD_OK;
}

MOD_EXPORT ModResult mod_shutdown(ModError*) {
    svc_log->info(mod_ctx, "time_sync_neo shutdown");
    return MOD_OK;
}
}
