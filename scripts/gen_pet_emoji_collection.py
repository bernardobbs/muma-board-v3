#!/usr/bin/env python3
"""Gera pet_emoji_collection.cc a partir de pet_gifs/<especie>_<humor>.gif.

O display do board (main/display/lcd_display.cc no 78/xiaozhi-esp32)
detecta um GIF de verdade pelos bytes magicos "GIF" e anima sozinho via
LvglGif -- nao precisamos de nenhum player customizado. Cada arquivo
aqui so precisa virar um array de bytes C, embutido num .cc normal
(pego pelo file(GLOB) automatico da pasta do board -- ver README.md).

Nome esperado: pet_gifs/<especie>_<humor>.gif
  especie: lobo, raposa, gato, dragao, unicornio, coelha, panda, pintinho
  humor:   neutral, thinking, surprised, funny, happy
           (as mesmas chaves de Tamagotchi::MoodName())

Rode sempre que adicionar/trocar GIFs em pet_gifs/:
    python3 scripts/gen_pet_emoji_collection.py
"""
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parent.parent
GIF_DIR = ROOT / "pet_gifs"
OUT = ROOT / "pet_emoji_collection.cc"

NAME_RE = re.compile(r"^([a-z]+)_([a-z]+)\.gif$")


def c_ident(species: str, mood: str) -> str:
    return f"k{species.capitalize()}_{mood}"


def bytes_to_c_array(name: str, data: bytes) -> str:
    lines = [f"static const unsigned char {name}[] = {{"]
    for i in range(0, len(data), 20):
        chunk = data[i:i + 20]
        lines.append("    " + ",".join(str(b) for b in chunk) + ",")
    lines.append("};")
    return "\n".join(lines)


def main() -> None:
    GIF_DIR.mkdir(exist_ok=True)
    gif_files = sorted(GIF_DIR.glob("*.gif"))

    by_species: dict[str, list[tuple[str, str]]] = {}
    arrays = []
    for path in gif_files:
        m = NAME_RE.match(path.name)
        if not m:
            print(f"pulando {path.name}: nome nao bate com <especie>_<humor>.gif")
            continue
        species, mood = m.group(1), m.group(2)
        ident = c_ident(species, mood)
        data = path.read_bytes()
        if data[:3] != b"GIF":
            print(f"AVISO: {path.name} nao comeca com os bytes magicos 'GIF' -- confirme o formato")
        arrays.append(bytes_to_c_array(ident, data))
        by_species.setdefault(species, []).append((mood, ident))

    out = [
        '#include "pet_emoji_collection.h"',
        "",
        "// GERADO por scripts/gen_pet_emoji_collection.py -- nao editar a mao.",
        "// Fonte real: pet_gifs/<especie>_<humor>.gif",
        "",
    ]

    if not by_species:
        # Sem GIFs ainda: nao declara array de tamanho zero (nao e C++
        # padrao) -- so devolve nullptr, quem chamar mantem o padrao.
        out.append("std::shared_ptr<EmojiCollection> CreatePetEmojiCollection(const std::string&) {")
        out.append("    return nullptr;")
        out.append("}")
        out.append("")
        OUT.write_text("\n".join(out), encoding="utf-8")
        print(f"pet_emoji_collection.cc atualizado: 0 gif(s), 0 especie(s) (stub)")
        return

    out.append('#include "display/lvgl_display/lvgl_image.h"')
    out.append("")
    out.append("namespace {")
    out.append("")
    out.append("struct GifAsset {")
    out.append("    const char* mood;")
    out.append("    const unsigned char* data;")
    out.append("    unsigned int size;")
    out.append("};")
    out.append("")
    out.append("struct SpeciesAssets {")
    out.append("    const char* id;")
    out.append("    const GifAsset* assets;")
    out.append("    unsigned int count;")
    out.append("};")
    out.append("")

    out.extend(arrays)
    out.append("")

    species_asset_vars = []
    for species, moods in sorted(by_species.items()):
        var_name = f"k{species.capitalize()}Assets"
        out.append(f"static const GifAsset {var_name}[] = {{")
        for mood, ident in moods:
            out.append(f'    {{"{mood}", {ident}, sizeof({ident})}},')
        out.append("};")
        out.append("")
        species_asset_vars.append((species, var_name, len(moods)))

    out.append("static const SpeciesAssets kAllSpecies[] = {")
    for species, var_name, count in species_asset_vars:
        out.append(f'    {{"{species}", {var_name}, {count}}},')
    out.append("};")
    out.append("")
    out.append("}  // namespace")
    out.append("")
    out.append("std::shared_ptr<EmojiCollection> CreatePetEmojiCollection(const std::string& species_id) {")
    out.append("    for (const auto& sp : kAllSpecies) {")
    out.append("        if (species_id == sp.id) {")
    out.append("            auto collection = std::make_shared<EmojiCollection>();")
    out.append("            for (unsigned int i = 0; i < sp.count; i++) {")
    out.append("                collection->AddEmoji(sp.assets[i].mood,")
    out.append("                    new LvglRawImage((void*)sp.assets[i].data, sp.assets[i].size));")
    out.append("            }")
    out.append("            return collection;")
    out.append("        }")
    out.append("    }")
    out.append("    return nullptr;  // especie sem GIFs ainda -- quem chamar mantem o padrao")
    out.append("}")
    out.append("")

    OUT.write_text("\n".join(out), encoding="utf-8")
    print(f"pet_emoji_collection.cc atualizado: {len(gif_files)} gif(s), {len(by_species)} especie(s)")


if __name__ == "__main__":
    main()
