"""Generate a real Farever world map from res.map.pak minimap tiles.

The UI/Window/Map PNGs in res.pak are zone preview screenshots and must not
be used as a coordinate map. This tool stitches Level/World/<world>.dat/
minimap/<tx>_<ty>_1024.png tiles from res.map.pak.
"""
from __future__ import annotations
import argparse, io, json, re, struct
from pathlib import Path
from PIL import Image

def load_pak(path):
    data=path.read_bytes()
    if data[:3] != b"PAK": raise RuntimeError("not a Farever pak")
    header_size=struct.unpack_from("<I",data,4)[0]
    entries=[]; off=12
    def walk(prefix):
        nonlocal off
        n=data[off]; off+=1
        name=data[off:off+n].decode("utf-8","replace"); off+=n
        flags=data[off]; off+=1
        full=(prefix+"/"+name) if prefix and name else (name or prefix)
        if flags & 1:
            count=struct.unpack_from("<I",data,off)[0]; off+=4
            for _ in range(count): walk(full)
        else:
            pos=struct.unpack_from("<d" if flags&2 else "<I",data,off)[0]
            off += 8 if flags&2 else 4
            size=struct.unpack_from("<I",data,off)[0]; off+=8
            entries.append((full,int(pos),size))
    walk("")
    return data,header_size,entries

def build(game, world, scale, output):
    pak=Path(game)/"res.map.pak"
    data,data_off,entries=load_pak(pak)
    pat=re.compile(r"Level/World/"+re.escape(world)+r"\.dat/minimap/(-?\d+)_(-?\d+)_1024\.png$",re.I)
    tiles={}
    for name,pos,size in entries:
        m=pat.search(name)
        if m: tiles[(int(m.group(1)),int(m.group(2)))] = data[data_off+pos:data_off+pos+size]
    if not tiles: raise RuntimeError("no minimap tiles for "+world)
    xs=[x for x,y in tiles]; ys=[y for x,y in tiles]
    x0,x1,y0,y1=min(xs),max(xs),min(ys),max(ys)
    tile_px=round(1024*scale)
    out=Image.new("RGB",((x1-x0+1)*tile_px,(y1-y0+1)*tile_px),(16,16,16))
    for (tx,ty),blob in tiles.items():
        im=Image.open(io.BytesIO(blob)).convert("RGB")
        if tile_px != 1024: im=im.resize((tile_px,tile_px),Image.Resampling.LANCZOS)
        out.paste(im,((tx-x0)*tile_px,(ty-y0)*tile_px))
    output.mkdir(parents=True,exist_ok=True)
    out_path=output/(world.lower()+".png")
    out.save(out_path,"PNG",optimize=True)
    meta={"world":world.lower(),"origin_x":x0*576.0,"origin_y":y0*576.0,
          "px_per_unit":tile_px/576.0,"width":out.width,"height":out.height,
          "units_per_tile":576.0,"tile_px":tile_px,
          "tiles_x":[x0,x1],"tiles_y":[y0,y1],"y_down":True}
    (output/(world.lower()+".json")).write_text(json.dumps(meta,indent=2),encoding="utf-8")
    print(out_path)
    print(json.dumps(meta,indent=2))

if __name__=="__main__":
    ap=argparse.ArgumentParser()
    ap.add_argument("--game",required=True)
    ap.add_argument("--world",default="W1_Siagarta")
    ap.add_argument("--scale",type=float,default=0.25)
    ap.add_argument("--output",type=Path,default=Path("modules/Patobeur/Map/assets/maps"))
    a=ap.parse_args()
    build(a.game,a.world,a.scale,a.output)