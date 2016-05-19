/**
 * atapp_module_impl.h
 *
 *  Created on: 2016年05月18日
 *      Author: owent
 */
#ifndef LIBATAPP_ATAPP_MODULE_IMPL_H_
#define LIBATAPP_ATAPP_MODULE_IMPL_H_

#pragma once

#include "std/explicit_declare.h"
#include "std/smart_ptr.h"

namespace atapp {
    class app;

    class module_impl {
    protected:
        module_impl();
        ~module_impl;

    private:
        module_impl(const module_impl &) FUNC_DELETE;
        const module_impl &operator=(const module_impl &) FUNC_DELETE;

    public:
        virtual int init() = 0;
        virtual int reload();
        virtual int stop();
        virtual const char *name();

        /**
         * @brief run tick handle and return active action number
         * @return active action number or error code
         */
        virtual int tick();

    private:
        app *owner_;

        friend class app;
    };
}

#endif
