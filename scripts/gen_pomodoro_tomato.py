#!/usr/bin/env python3
"""Gera pomodoro_tomato.cc a partir de pomodoro_tomato.png.

Mesma tecnica do gen_pet_emoji_collection.py: embute os bytes crus do PNG
num array C, e o LvglRawImage (main/display/lvgl_display/lvgl_image.h,
78/xiaozhi-esp32) decodifica via LV_COLOR_FORMAT_RAW_ALPHA -- o mesmo
mecanismo que ja usa pro pacote de emoji padrao (PNGs) e pros GIFs
customizados do bichinho. Sem pipeline de asset novo nenhum.

Rode sempre que regenerar pomodoro_tomato.png:
    python3 scripts/gen_pomodoro_tomato.py
"""
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
PNG = ROOT / "pomodoro_tomato.png"
OUT = ROOT / "pomodoro_tomato.cc"


def bytes_to_c_array(name: str, data: bytes) -> str:
    lines = [f"static const unsigned char {name}[] = {{"]
    for i in range(0, len(data), 20):
        chunk = data[i:i + 20]
        lines.append("    " + ",".join(str(b) for b in chunk) + ",")
    lines.append("};")
    return "\n".join(lines)


def main() -> None:
    data = PNG.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        print(f"AVISO: {PNG.name} nao comeca com os bytes magicos de PNG -- confirme o formato")

    out = [
        '#include "pomodoro_tomato.h"',
        "",
        "// GERADO por scripts/gen_pomodoro_tomato.py -- nao editar a mao.",
        "// Fonte real: pomodoro_tomato.png",
        "",
        bytes_to_c_array("kPomodoroTomatoPng", data),
        "",
        "std::shared_ptr<LvglImage> GetPomodoroTomatoImage() {",
        "    static std::shared_ptr<LvglImage> img =",
        "        std::make_shared<LvglRawImage>((void*)kPomodoroTomatoPng, sizeof(kPomodoroTomatoPng));",
        "    return img;",
        "}",
        "",
    ]
    OUT.write_text("\n".join(out), encoding="utf-8")
    print(f"pomodoro_tomato.cc atualizado: {len(data)} bytes de PNG embutidos")


if __name__ == "__main__":
    main()
