
import { assert } from "console";
import { format } from "util";
import { readFile } from "fs/promises";
import { exit } from "process";

export class Parser {

  constructor(data, pos = 0) {
    this.data = data;
    this.pos = pos;
  }

  nextToken() {

    while (this.isSpace()) {
      this.pos++;
    }

    if (this.isEof()) {
      return "";
    }

    let start = this.pos, end = this.pos;

    if (this.isQuote()) {
      this.pos++;
      start++;

      while (!this.isQuote()) {
        this.pos++;
        if (this.isEof()) {
          throw new Error(`Unmatched quote at ${this.pos}: ${this.data.substring(start)}`);
        }
      }
      end = this.pos++;
    } else {
      while (!this.isSpace()) {
        this.pos++;
        if (this.isEof()) {
          break;
        }
      }
      end = this.pos;
    }

    return this.data.substring(start, end);
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

  nextLine() {
    const start = this.pos;

    while (!this.isNewLine()) {
      this.pos++;
    }

    const end = this.pos++;

    return this.data.substring(start, end);
  }

  peek() {
    return new Parser(this.data, this.pos).nextToken();
  }

  skip(token) {

    if (this.peek().startsWith(token)) {
      while (this.isSpace()) {
        this.pos++;
      }
      
      this.pos += token.length;
    }
  }

  assert(token) {
    assert(this.nextToken() === token);
  }

  isNewLine() {
    return "\n\r".indexOf(this.data[this.pos]) != -1;
  }

  isSpace() {
    return " \t\n\r".indexOf(this.data[this.pos]) != -1;
  }

  isQuote() {
    return "\"".indexOf(this.data[this.pos]) != -1;
  }

  isEof() {
    return this.pos >= this.data.length;
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

    this.translate = [
      parser.nextNumber(),
      parser.nextNumber()
    ];

    this.rotate = parser.nextNumber();

    this.scale = [
      parser.nextNumber(),
      parser.nextNumber()
    ];
  }

  convert() {
    const that = Object.assign(Object.create(Side.prototype), {
      ...this,
      texture: this.texture.toLowerCase().replace("*", "#")
    });

    switch (that.texture) {
      case "clip":
      case "trigger":
        return Object.assign(Object.create(Side.prototype), {
          ...this,
          texture: `common/${that.texture}`,
          rotate: 0,
          translate: [0, 0],
          scale: [.5, .5]
        });
      case "sky":
      case "sky1":
      case "sky2":
      case "sky3":
      case "sky4":
        return Object.assign(Object.create(Side.prototype), {
          ...this,
          texture: "common/sky",
          rotate: 0,
          translate: [0, 0],
          scale: [.5, .5]
        });
      default:
        return Object.assign(Object.create(Side.prototype), {
          ...this,
          texture: `rygel/${that.texture}`,
          translate: this.translate.map(t => t / 16),
          scale: this.scale.map(s => s / 16)
        });
    }
  }

  write(stream) {
    stream.write(format("  ( %d %d %d ) ( %d %d %d ) ( %d %d %d ) %s %f %f %f %f %f\n",
      this.points[0][0], this.points[0][1], this.points[0][2],
      this.points[1][0], this.points[1][1], this.points[1][2],
      this.points[2][0], this.points[2][1], this.points[2][2],
      this.texture,
      this.translate[0], this.translate[1],
      this.rotate, 
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

  translate(delta) {
    this.origin = this.origin.split(" ")
      .map((s, i) => new Number(s) + delta[i])
      .join(" ");
  }

  convert() {
    const that = Object.assign(Object.create(Entity.prototype), {
      ...this,
      brushes: this.brushes.map(brush => brush.convert())
    });

    delete that.spawnflags;

    if (that.classname.startsWith("item_") || 
        that.classname.startsWith("weapon_")) {
      this.translate([0, 0, 16]);
    }

    switch (this.classname.toLowerCase()) {
      case "ambient_drip":
        return Object.assign(that, {
          classname: "misc_sound",
          sound: "common/drip"
        });
      case "info_intermission":
        return Object.assign(that, {
          classname: "info_player_intermission"
        });
      case "item_armor1":
        return Object.assign(that, {
          classname: "armor_jacket"
        });
      case "item_armor2":
        return Object.assign(that, {
          classname: "armor_combat"
        });
      case "item_cells":
        return Object.assign(that, {
          classname: "ammo_bolts"
        });
      case "item_health":
        switch (this.spawnflags) {
          case 1: 
            return Object.assign(that, {
              classname: "item_health"
            });
          case 2: 
            return Object.assign(that, {
              classname: "item_health_mega"
            });
          default:
            return that;
        }
      case "item_rockets":
        return Object.assign(that, {
          classname: "ammo_rockets"
        });
      case "item_shells":
        return Object.assign(that, {
          classname: "ammo_shells"
        });
      case "item_spikes":
        return Object.assign(that, {
          classname: "ammo_bullets"
        });
      case "light":
        return [];
      case "light_torch_small_walltorch":
        return Object.assign(that, {
          classname: "misc_flame"
        });
      case "light_flame_large_yellow":
        return Object.assign(that, {
          classname: "misc_flame",
          radius: 32
        });
      case "info_teleport_destination":
        delete that.light;
        return Object.assign(that, {
          classname: "misc_teleporter_dest"
        });
      case "weapon_nailgun":
        return Object.assign(that, {
          classname: "weapon_machinegun"
        });
      case "weapon_supernailgun":
        return Object.assign(that, {
          classname: "weapon_hyperblaster"
        });
      case "worldspawn":
        delete that.wad;
        delete that.worldtype;
        delete that.sounds;
        return Object.assign(that, {
          ambient: ".05 .05 .05",
          brightness: "1",
          give: "shotgun",
          sky: "sky1",
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

export class Light {

  constructor(parser) {

    parser.skip("!");

    this.origin = [
      parser.nextNumber(),
      parser.nextNumber(),
      parser.nextNumber()
    ];

    this.radius = parser.nextNumber();

    this.color = [
      parser.nextNumber(),
      parser.nextNumber(),
      parser.nextNumber()
    ];

    parser.nextLine();
  }

  write(stream) {
    stream.write("{\n");
    stream.write(format(` "classname" "light"\n`));
    stream.write(format(` "origin" "%f %f %f"\n`, this.origin[0], this.origin[1], this.origin[2]));
    stream.write(format(` "light" "%f"\n`, this.radius));
    stream.write(format(` "_color" "%f %f %f"\n`, this.color[0], this.color[1], this.color[2]));
    stream.write("}\n");
  }
}

export class Map {

  constructor(args) {
    [this.map, this.rtlights] = args;
    if (!this.rtlights) {
      this.rtlights = this.map.replace(".map", ".rtlights");
    }
  }

  async read() {
    this.entities = [];
    this.lights = [];

    try {
      const data = await readFile(this.map, { "encoding": "utf-8" });
      const parser = new Parser(data);
      
      while (parser.peek() == "{") {
        this.entities.push(new Entity(parser));
      }
    } catch (err) {
      console.error(`Failed to load ${this.map}`, err);
      exit(1);
    }

    try {
      const data = await readFile(this.rtlights, { "encoding": "utf-8" });
      const parser = new Parser(data);
      
      while (!parser.isEof()) {
        this.lights.push(new Light(parser));
      }
    } catch (err) {
      console.warn(`Failed to load ${this.rtlights}`, err);
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
    this.lights.forEach(l => l.write(stream));
  }
}
