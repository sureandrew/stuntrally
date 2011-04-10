#!/usr/bin/python

import sys

def usage():
	print "Usage: " + sys.argv[0] + " someFile.xml someFile.pot"
	print "Usage: " + sys.argv[0] + " someFile.po someFile.xml"

if (len(sys.argv)) != 3:
	usage()
	sys.exit(0)

file1 = sys.argv[1]
file2 = sys.argv[2]

f1 = open(file1, 'r')
	
f2 = open(file2, 'w')

msgs = {}
# msgs = {"foo":"bar", "asdf":"ghij"}

if file1.endswith(".xml") and file2.endswith(".pot"):
	#xml2po
	
	# read xml
	for line in f1:
		if line.strip().startswith("<Tag name="):
			msgid = line.split("=\"")[1].split("\"")[0]
			msgstr = line.split(">")[1].split("<")[0]
			msgs[msgid] = msgstr
			
	# write po
	
	# header
	# TODO
	
	# translations
	result = ""
	for mid, mstr in msgs.items():
		result += "msgid \"" + mid + "\"\n"
		result += "msgstr \"\"\n"
		result += "\n"
		
	f2.write(result)
	
elif file1.endswith(".po") and file2.endswith(".xml"):
	#po2xml
	
	msgid = ""
	msgstr = ""
	last = "msgstr"
	# read po
	for line in f1:
		if line.strip().startswith("\""):
			# multi-line
			# TODO
			if last == "msgid":
				msgid += line.strip().split("\"")[1].replace("\\n", "\n")
			elif last == "msgstr":
				msgstr += line.strip().split("\"")[1].replace("\\n", "\n")
		if line.strip().startswith("msgid"):
			# add previous msgstr
			if msgid != "":
				msgs[msgid] = msgstr
			
			msgid = line.split("\"")[1].replace("\\n", "\n")
			last = "msgid"
		if line.strip().startswith("msgstr"):
			msgstr =line.split("\"")[1].replace("\\n", "\n")
			last = "msgstr"

	print msgs
		
else:
	usage()

f1.close()
f2.close()