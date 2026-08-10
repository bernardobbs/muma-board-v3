#!/usr/bin/env python3
"""Gera web_assets.cc a partir de www/index.html e www/admin.html.

O build do xiaozhi-esp32 nao tem gancho de EMBED_TXTFILES por board
(so um idf_component_register unico pro projeto inteiro), entao o HTML
vira string C++ em vez de arquivo embutido. Rode isto sempre que editar
www/index.html ou www/admin.html:

    python3 scripts/gen_web_assets.py
"""
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
DELIM = "MUMA_HTML"


def read_checked(path: pathlib.Path) -> str:
    content = path.read_text(encoding="utf-8")
    closing = ")" + DELIM + '"'
    if closing in content:
        raise SystemExit(f"{path}: contem '{closing}', escolha outro delimitador")
    return content


def literal(name: str, content: str) -> str:
    return f'const char {name}[] = R"{DELIM}(\n{content}){DELIM}";'


def main() -> None:
    idx = read_checked(ROOT / "www" / "index.html")
    adm = read_checked(ROOT / "www" / "admin.html")

    out = (
        '#include "web_assets.h"\n\n'
        "// GERADO por scripts/gen_web_assets.py -- nao editar a mao.\n"
        "// Fonte real: www/index.html e www/admin.html.\n\n"
        + literal("kIndexHtml", idx) + "\n\n"
        + literal("kAdminHtml", adm) + "\n"
    )
    (ROOT / "web_assets.cc").write_text(out, encoding="utf-8")
    print("web_assets.cc atualizado")


if __name__ == "__main__":
    main()
