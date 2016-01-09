/***
 Copyright (C) 2015 Intel Corporation

 Author: Julio Montes <julio.montes@intel.com>

 This file is part of clr-cloud-init.

 clr-cloud-init is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 clr-cloud-init is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with clr-cloud-init. If not, see <http://www.gnu.org/licenses/>.

 In addition, as a special exception, the copyright holders give
 permission to link the code of portions of this program with the
 OpenSSL library under certain conditions as described in each
 individual source file, and distribute linked combinations
 including the two.
 You must obey the GNU General Public License in all respects
 for all of the code used other than OpenSSL.  If you modify
 file(s) with this exception, you may extend this exception to your
 version of the file(s), but you are not obligated to do so.  If you
 do not wish to do so, delete this exception statement from your
 version.  If you delete this exception statement from all source
 files in the program, then also delete it here.
***/

#pragma once

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif

#ifndef DEFAULT_USER_HOME_DIR
	#define DEFAULT_USER_HOME_DIR "/home/clear"
#endif /* DEFAULT_USER_HOME_DIR */

#ifndef DEFAULT_USER_GROUPS
	#define DEFAULT_USER_GROUPS "wheel"
#endif /* DEFAULT_USER_GROUPS */

#ifndef DEFAULT_USER_INACTIVE
	#define DEFAULT_USER_INACTIVE "-1"
#endif /* DEFAULT_USER_INACTIVE */

#ifndef DEFAULT_USER_EXPIREDATE
	#define DEFAULT_USER_EXPIREDATE ""
#endif /* DEFAULT_USER_EXPIREDATE */

#ifndef DEFAULT_USER_SHELL
	#define DEFAULT_USER_SHELL "/bin/bash"
#endif /* DEFAULT_USER_SHELL */

#ifndef DEFAULT_USER_USERNAME
	#define DEFAULT_USER_USERNAME "clear"
#endif /* DEFAULT_USER_USERNAME */

#ifndef DEFAULT_USER_GECOS
	#define DEFAULT_USER_GECOS "Clear Linux"
#endif /* DEFAULT_USER_GECOS */

#ifndef DEFAULT_USER_PASSWORD
	#define DEFAULT_USER_PASSWORD "!"
#endif /* DEFAULT_USER_PASSWORD */

#ifndef DEFAULT_USER_SUDO
	#define DEFAULT_USER_SUDO DEFAULT_USER_USERNAME " ALL=(ALL) NOPASSWD:ALL"
#endif /* DEFAULT_USER_SUDO */
