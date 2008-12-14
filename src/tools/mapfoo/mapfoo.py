#!/usr/bin/python

import sys

class Vert:
	"""
	A vertex, or coordinate in 3D space.  Faces have 3 Verts.
	"""
	
	def __init__(self, v):
		(self.x, self.y, self.z) = (v[0], v[1], v[2])
	
	def writeTo(self, stream):
		stream.write("( %s %s %s ) " % (self.x, self.y, self.z))

class Face:
	"""
	A face is defined by 3 vertexes and several attributes, such as texture
	alignment, surface flags, and a contents mask.
	"""
	
	vals = "0 0 0 0.5 0.5 0 0 0".split()
	
	def __init__(self, string):
		
		self.verts, self.tex = [], "common/notex"
		
		arr = string.split()
		
		self.verts.append(Vert(arr[1:4]))
		self.verts.append(Vert(arr[6:9]))
		self.verts.append(Vert(arr[11:14]))
		
		length = len(arr)
		
		"""
		Standard faces are 24 tokens, with texture at index 15.  WorldCraft
		faces are 31 tokens, with texture at index also at 15.  Finally, 
		BrushDef style faces are 31 tokens, with texture at index 27.
		"""
		
		if length == 24:
			self.tex = arr[15]
			self.vals = arr[16:]
		elif length == 31:
			if arr[16] == "[":
				self.tex = arr[15]
			else:
				self.tex = arr[27]
	
	def writeTo(self, stream):
		
		for v in self.verts:
			v.writeTo(stream)
		
		stream.write("%s " % self.tex.lower())
		stream.write(" ".join(self.vals))
		stream.write("\n")

class Brush:
	"""
	A brush, or convex volume.  Brushes are defined by 4 or more faces.
	Patches, or curves, are also parsed as brushes, but are discarded.
	"""
	
	def toPatch(self, file):
		
		self.isPatch = True
		
		while True:
			try:
				line = file.next().strip()
			except StopIteration:
				return
			
			if line.startswith("}"):
				return
	
	def __init__(self, file):
		(self.faces, self.braces, self.isPatch) = ([], 0, False)
		
		while True:
			try:
				line = file.next().strip()
			except StopIteration:
				break
			
			if line.startswith("("):
				self.faces.append(Face(line))
			
			elif line.startswith("}"):
				if self.braces == 0:
					return
				self.braces -= 1
			
			elif line.startswith("brushDef"):
				self.braces += 1
			
			elif line.startswith("patchDef"):
				self.toPatch(file)
	
	def writeTo(self, stream):
		
		if self.isPatch:
			return
		
		stream.write("// brush\n{\n")
		
		for f in self.faces:
			f.writeTo(stream)
		
		stream.write("}\n")

class Entity:
	"""
	Entities are the core elements of map files.  An entity is defined by 0 or
	more key-value pairs, and 0 or more brushes.  All map files have at least
	one entity with classname "worldspawn".
	"""
	
	def __init__(self, file):
		(self.pairs, self.brushes) = ({}, [])
		
		while True:
			try:
				line = file.next().strip()
			except StopIteration:
				break
			
			if line.startswith("{"):
				self.brushes.append(Brush(file))
			
			elif line.startswith("\""):
				self.parsePair(line)
			
			elif line.startswith("}"):
				return
	
	def parsePair(self, line):
		
		arr = line.split("\"")
		
		self.pairs[arr[1]] = arr[3]
	
	def writeTo(self, stream):
		
		stream.write("// entity\n{\n")
		
		for p in self.pairs:
			stream.write("\"%s\" \"%s\"\n" % (p, self.pairs[p]))
		
		for b in self.brushes:
			b.writeTo(stream)
		
		stream.write("}\n")

class Map:
	"""
	A map file.  Maps are defined by 1 or more entities.
	"""
	
	def __init__(self, file):
		self.entities = []
		
		while True:
			try:
				line = file.next().strip()
			except StopIteration:
				break
			
			if line.startswith("{"):
				self.entities.append(Entity(file))
	
	def counts(self):
		"""
		Returns a three-tuple containing counts of entities, brushes, and
		patches which comprise this Map.
		"""
		(num_entities, num_brushes, num_patches) = (len(self.entities), 0, 0)
		
		for e in self.entities:
			for b in e.brushes:
				if b.isPatch:
					num_patches += 1
				else:
					num_brushes += 1
		
		return (num_entities, num_brushes, num_patches)
	
	def writeTo(self, stream):
		
		for e in self.entities:
			e.writeTo(stream)

def main(argc, argv):
	"""
	Resolve input file and output stream, then parse the map.
	"""
	
	if argc < 2 or argc > 3:
		print "Usage: %s in.map [out.map]" % argv[0]
		sys.exit(1)
	
	file = open(argv[1], "r")
	
	if argc == 3:
		stream = open(argv[2], "w")
	else:
		stream = sys.stdout
	
	map = Map(file)
	
	print "Parsed %s entities, %s brushes, %s patches" % map.counts()
	
	map.writeTo(stream)
	stream.close()

if __name__ == "__main__":
	main(len(sys.argv), sys.argv)

