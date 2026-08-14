# GIFs do bichinho

Coloque aqui os GIFs animados, um por espécie×humor:

```
pet_gifs/<especie>_<humor>.gif
```

- `especie`: `lobo`, `raposa`, `gato`, `dragao`, `unicornio`, `coelha`,
  `panda`, `pintinho` (mesmos ids do catálogo em `tamagotchi_tool.cc`).
- `humor`: `neutral`, `thinking`, `surprised`, `funny`, `happy`
  (mesmas chaves de `Tamagotchi::MoodName()`).

Depois de adicionar/trocar arquivos aqui, rode:

```bash
python3 ../scripts/gen_pet_emoji_collection.py
```

Isso regenera `pet_emoji_collection.cc` (a partir daqui) com os bytes
de cada GIF embutidos como array C -- pego automaticamente pelo build
do board, sem precisar editar nenhum `CMakeLists.txt`.

Espécie sem GIF aqui continua funcionando normalmente: o aparelho usa
o pacote de emoji padrão do xiaozhi (rosto genérico reagindo ao
humor), só não mostra a arte customizada dessa espécie.
