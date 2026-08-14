#pragma once
#include "display/lvgl_display/emoji_collection.h"
#include <memory>
#include <string>

// Colecao de emoji CUSTOMIZADA por especie do bichinho -- GIFs animados
// (um por humor) em vez do rosto generico padrao do xiaozhi. As chaves
// de humor batem com Tamagotchi::MoodName() ("neutral", "thinking",
// "surprised", "funny", "happy").
//
// Devolve nullptr se a especie ainda nao tiver GIFs gerados -- nesse
// caso quem chamar deve manter o EmojiCollection padrao (rosto
// generico), nunca travar por falta de arte.
std::shared_ptr<EmojiCollection> CreatePetEmojiCollection(const std::string& species_id);
