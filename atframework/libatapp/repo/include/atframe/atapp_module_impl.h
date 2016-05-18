/**
 * atapp.h
 *
 *  Created on: 2016年05月18日
 *      Author: owent
 */
#ifndef LIBATAPP_ATAPP_MODULE_IMPL_H_
#define LIBATAPP_ATAPP_MODULE_IMPL_H_

#pragma once

#include "design_pattern/noncopyable.h"

namespace atapp {
    class module_impl : public util::design_pattern::noncopyable {
    protected:
        module_impl();
        ~module_impl;

    public:
        virtual int init() = 0;
        virtual int reload();
        virtual int stop();
        virtual int tick();
    };
}

#endif
