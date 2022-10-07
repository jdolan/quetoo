
import { assert } from "console";
import { format } from "util";
import { readFile } from "fs/promises";

export class Parser {

  constructor(data) {
    this.data = data;
    this.pos = 0;
    this.token = null;
  }

  nextToken() {
    while (this.isSpace()) {
      this.pos++;
    }

    if (this.isQuote()) {
      this.pos++;

      const start = this.pos;

      while (!this.isQuote()) {
        this.pos++;
      }

      const end = this.pos++;

      this.token = this.data.substring(start, end);
      return this.token;
    }
  
    const start = this.pos;

    while (!this.isSpace()) {
      this.pos++;
    }

    const end = this.pos++;

    this.token = this.data.substring(start, end);
    return this.token;
  }

  nextNumber() {
    return new Number(this.nextToken());
  }

  nextPoint() {
    this.assert("(");
    const point = [this.nextNumber(), this.nextNumber(), this.nextNumber()];
    this.assert(")");
    return point;
  }

  peek() {
    const pos = this.pos;
    const token = this.nextToken();
    this.pos = pos;
    return token;
  }

  assert(token) {
    assert(this.nextToken() === token);
  }

  isSpace() {
    return " \t\n".indexOf(this.data[this.pos]) != -1;
  }

  isQuote() {
    return "\"".indexOf(this.data[this.pos]) != -1;
  }

  isEof() {
    return this.pos == this.data.length;
  }
}

export class Side {

  constructor(parser) {
    this.points = [
      parser.nextPoint(), 
      parser.nextPoint(), 
      parser.nextPoint()
    ];

    this.texture = parser.nextToken();

    this.rotate = parser.nextNumber();

    this.translate = [
      parser.nextNumber(),
      parser.nextNumber()
    ];

    this.scale = [
      parser.nextNumber(),
      parser.nextNumber()
    ];
  }

  convert() {
    switch (this.texture.toLowerCase()) {
      case "clip":
      case "sky":
      case "trigger":
        return Object.assign(Object.create(Side.prototype), {
          ...this,
          texture: `common/${this.texture.toLowerCase()}`,
          rotate: 0,
          translate: [0, 0],
          scale: [.5, .5]
        });
      default:
        return Object.assign(Object.create(Side.prototype), {
          ...this,
          texture: `rygel/${this.texture.toLowerCase()}`,
          translate: this.translate.map(t => t / 16),
          scale: this.scale.map(s => s / 16)
        });
    }
  }

  write(stream) {
    stream.write(format(" ( %d %d %d ) ( %d %d %d ) ( %d %d %d ) %s %f %f %f %f %f\n",
      this.points[0][0], this.points[0][1], this.points[0][2],
      this.points[1][0], this.points[1][1], this.points[1][2],
      this.points[2][0], this.points[2][1], this.points[2][2],
      this.texture,
      this.rotate, 
      this.translate[0], this.translate[1], 
      this.scale[0], this.scale[1]
    ));
  }
}

export class Brush {

  constructor(parser) {    
    this.sides = [];
    parser.assert("{");

    while (parser.peek() != "}") {
      this.sides.push(new Side(parser));
    }

    parser.assert("}");
  }

  convert() {
    return Object.assign(Object.create(Brush.prototype), {
      ...this,
      sides: this.sides.map(side => side.convert())
    });
  }

  write(stream) {
    stream.write(" {\n");
    this.sides.forEach(side => side.write(stream));
    stream.write(" }\n");
  }
}

export class Entity {

  constructor(parser) {
    this.brushes = [];

    parser.assert("{");

    while (parser.peek() !== "}") {
      if (parser.peek() === "{") {
        this.brushes.push(new Brush(parser));
      } else {
        this[parser.nextToken()] = parser.nextToken();
      }
    }

    parser.assert("}");
  }

  convert() {
    const that = Object.assign(Object.create(Entity.prototype), {
      ...this,
      brushes: this.brushes.map(brush => brush.convert())
    });

    switch (this.classname) {
      case "light":
      case "light_torch_small_walltorch":
        return [];
      case "info_teleport_destination":
        return Object.assign(that, {
          classname: "misc_teleporter_dest"
        });
      default:
        return that;
    }
  }

  write(stream) {
    stream.write("{\n");
    for (const key in this) {
      if (typeof(this[key]) === "string") {
        stream.write(` "${key}" "${this[key]}"\n`);
      }
    }
    this.brushes.forEach(brush => brush.write(stream));
    stream.write("}\n");
  }
}

export class Map {

  constructor(path) {
    this.path = path;
  }

  async read() {
    this.entities = [];

    const data = await readFile(this.path, { "encoding": "utf-8" });
    const parser = new Parser(data);
    
    while (!parser.isEof()) {
      this.entities.push(new Entity(parser));
    }
  }

  convert() {
    return Object.assign(Object.create(Map.prototype), {
      ...this,
      entities: this.entities.flatMap(e => e.convert())
    });
  }

  write(stream) {
    this.entities.forEach(e => e.write(stream));
  }
}