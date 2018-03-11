#!/usr/bin/python

import Cocoa
import sys

icon = sys.argv[1].decode('utf-8')
image = Cocoa.NSImage.alloc().initWithContentsOfFile_(icon)

file_or_folder = sys.argv[2].decode('utf-8')

Cocoa.NSWorkspace.sharedWorkspace().setIcon_forFile_options_(image, file_or_folder, 0) or sys.exit("Unable to set file icon")
