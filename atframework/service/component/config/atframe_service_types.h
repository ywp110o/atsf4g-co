#ifndef _ATFRAME_SERVICE_COMPONENT_CONFIG_SERVICE_TYPES_H_
#define _ATFRAME_SERVICE_COMPONENT_CONFIG_SERVICE_TYPES_H_

#pragma once

namespace atframe {
    namespace component {
        struct service_type {
            enum type {
                EN_ATST_UNKNOWN = 0,
                EN_ATST_ATPROXY,
                EN_ATST_GATEWAY,

                EN_ATST_INNER_BOUND = 0x80,
                EN_ATST_CUSTOM_START = 0x81,
            };
        };
    }
}
#endif