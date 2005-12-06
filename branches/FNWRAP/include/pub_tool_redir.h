
/*--------------------------------------------------------------------*/
/*--- Redirections, etc.                          pub_tool_redir.h ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2000-2005 Julian Seward
      jseward@acm.org

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.

   The GNU General Public License is contained in the file COPYING.
*/

#ifndef __PUB_TOOL_REDIR_H
#define __PUB_TOOL_REDIR_H

/* The following macros facilitate function redirection (redirects).

   The general idea is: you can write a function like this:

      ret_type 
      VG_REDIRECT_FUNCTION_ZU(zEncodedSoname,fnname) ( .. args .. )
      {
         ... body ...
      }

   zEncodedSoname should be a Z-encoded soname (see below for Z-encoding
   details) and fnname should be an unencoded fn name.  The resulting name is

      _vgrZU_zEncodedSoname_fnname

   The "_vgrZU_" is a prefix that gets discarded upon decoding.

   It is also possible to write

      ret_type 
      VG_REDIRECT_FUNCTION_ZZ(zEncodedSoname,zEncodedFnname) ( .. args .. )
      {
         ... body ...
      }
   
   which means precisely the same, but the function name is also
   Z-encoded.  This can sometimes be necessary.  In this case the
   resulting function name is

      _vgrZZ_zEncodedSoname_zEncodedFnname

   When it sees this either such name, the core's symbol-table reading
   machinery and redirection machinery first Z-decode the soname and 
   if necessary the fnname.  They are encoded so that they may include
   arbitrary characters, and in particular they may contain '*', which
   acts as a wildcard.

   They then will conspire to cause calls to any function matching
   'fnname' in any object whose soname matches 'soname' to actually be
   routed to this function.  This is used in Valgrind to define dozens
   of replacements of malloc, free, etc.

   The soname must be a Z-encoded bit of text because sonames can
   contain dots etc which are not valid symbol names.  The function
   name may or may not be Z-encoded: to include wildcards it has to be,
   but Z-encoding C++ function names which are themselves already mangled
   using Zs in some way is tedious and error prone, so the _ZU variant
   allows them not to be Z-encoded.

   Note that the soname "NONE" is specially interpreted to match any
   shared object which doesn't have a soname.

   Note also that the replacement function should probably (must be?) in
   client space, so it runs on the simulated CPU.  So it must be in
   either vgpreload_<tool>.so or vgpreload_core.so.  It also only works
   with functions in shared objects, I think.
   
   It is important that the Z-encoded names contain no unencoded
   underscores, since the intercept-handlers in m_redir.c detect the
   end of the soname by looking for the first trailing underscore.

   Z-encoding details: the scheme is like GHC's.  It is just about
   readable enough to make a preprocessor unnecessary.  First the
   "_vgrZU_" or "_vgrZZ_" prefix is added, and then the following
   characters are transformed.

     *         -->  Za    (asterisk)
     +         -->  Zp    (plus)
     :         -->  Zc    (colon)
     .         -->  Zd    (dot)
     _         -->  Zu    (underscore)
     -         -->  Zh    (hyphen)
     (space)   -->  Zs    (space)
     @         ->   ZA    (at)
     Z         -->  ZZ    (Z)

   Everything else is left unchanged.
*/

/* If you change these, the code in VG_(maybe_Z_demangle) needs to
   be changed accordingly. */
#define VG_REDIRECT_FUNCTION_ZU(soname,fnname) _vgrZU_##soname##_##fnname
#define VG_REDIRECT_FUNCTION_ZZ(soname,fnname) _vgrZZ_##soname##_##fnname


#endif   // __PUB_TOOL_REDIR_H

/*--------------------------------------------------------------------*/
/*--- end                                                          ---*/
/*--------------------------------------------------------------------*/
