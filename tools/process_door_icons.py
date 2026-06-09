from PIL import Image
from pathlib import Path

TARGET = 512
PAD_RATIO = 0.03
RED = (229, 57, 53)
WHITE = (255, 255, 255)


def is_background_pixel(r: int, g: int, b: int, a: int) -> bool:
    if a < 16:
        return True
    if r >= 235 and g >= 235 and b >= 235:
        return True
    if 170 <= r <= 240 and 170 <= g <= 240 and 170 <= b <= 240:
        return True
    return False


def colorize(src: Path, line_rgb: tuple[int, int, int]) -> Image.Image:
    img = Image.open(src).convert("RGBA")
    px = img.load()
    w, h = img.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if is_background_pixel(r, g, b, a):
                px[x, y] = (0, 0, 0, 0)
            else:
                strength = 255 - min(r, g, b)
                alpha = max(0, min(255, int(strength * 1.35)))
                px[x, y] = (*line_rgb, alpha) if alpha >= 20 else (0, 0, 0, 0)
    return img


def trim_and_fill_width(
    img: Image.Image,
    target: int = TARGET,
    pad_ratio: float = PAD_RATIO,
) -> Image.Image:
    bbox = img.getbbox()
    if not bbox:
        return img
    cropped = img.crop(bbox)
    cw, ch = cropped.size
    pad = max(1, int(target * pad_ratio))
    inner = target - pad * 2
    nw = inner
    nh = max(1, int(ch * inner / cw))
    scaled = cropped.resize((nw, nh), Image.Resampling.LANCZOS)
    if nh > inner:
        top = (nh - inner) // 2
        scaled = scaled.crop((0, top, nw, top + inner))
    out = Image.new("RGBA", (target, target), (0, 0, 0, 0))
    out.paste(scaled, (pad, pad), scaled)
    return out


def save_png(img: Image.Image, dst: Path) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    img.save(dst, "PNG")
    print(f"wrote {dst}")


def process_file(src: Path, dst: Path, line_rgb: tuple[int, int, int]) -> None:
    img = colorize(src, line_rgb)
    save_png(trim_and_fill_width(img), dst)


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    material_dir = root / "docs" / "素材"
    out_dir = root / "app" / "src" / "main" / "res" / "drawable-nodpi"

    process_file(material_dir / "关门.jpeg", out_dir / "ic_door_closed.png", WHITE)
    process_file(material_dir / "开门.jpeg", out_dir / "ic_door_open.png", RED)


if __name__ == "__main__":
    main()
