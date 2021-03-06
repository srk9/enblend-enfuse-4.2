/*
 * Copyright (C) 2009-2016 Dr. Christoph L. Spiel
 *
 * This file is part of Enblend.
 *
 * Enblend is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Enblend is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Enblend; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include <cstdio>              // fflush(), vsnprintf()
#include <cstring>
#include <iostream>
#include <set>
#include <string>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "global.h"
#include "tiff_message.h"


extern const std::string command;
extern int Verbose;

static std::set<std::string> tiff_messages;


static std::string
interpolate_format(const char* format, va_list arguments)
{
    const size_t buffer_size = 4096U;
    std::string message(buffer_size, '\0');

    vsnprintf(&message[0], buffer_size, format, arguments);

    return message;
}


static void
flush_buffers()
{
    fflush(stdout);
    std::cout.flush();
    std::wcout.flush();
}


/** tiff_message mangles the error and warning messages from the TIFF
 *  library to make them look more like Enblend/Enfuse messages.
 *
 *  For the messages tend to occur repeatedly, we keep track of every
 *  message and only pass on their first appearance.  The library
 *  always includes the name of the offending TIFF file, thus we get
 *  each message once for each file. */
static void
tiff_message(const char* message_class,
             const char* /* module */, const char* format, va_list arguments)
{
    const std::string message(interpolate_format(format, arguments));

    if (tiff_messages.count(message) == 0)
    {
        tiff_messages.insert(message);

        // IMPLEMENTATION NOTE: We do not know, where we are called,
        // therefore we must flush all buffers before we print our
        // message.
        flush_buffers();

        std::cerr << command << ": " << message_class << ": " << message << std::endl;
    }
}


void
tiff_warning(const char* module, const char* format, va_list arguments)
{
    if (Verbose >= VERBOSE_TIFF_MESSAGES)
    {
        tiff_message("warning", module, format, arguments);
    }
}


static bool
is_benign(const char* format, va_list arguments)
{
    const std::string message(interpolate_format(format, arguments));

    return message == "OJPEG encoding not supported; use new-style JPEG compression instead";
}


void
tiff_error(const char* module, const char* format, va_list arguments)
{
    tiff_message(is_benign(format, arguments) ? "warning" : "error", module, format, arguments);
}
