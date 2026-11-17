# Preparação do repositório para o GitHub — 17/11/2026

## Alterações

- Reescrito o README como página inicial em pt-BR, com apresentação do Termuni, status, recursos, requisitos, build, testes, mapa de diretórios, documentação e licença.
- Mantida a estrutura existente, que já separava aplicação (`src/`), recursos (`data/`), testes (`tests/`), ferramentas (`tools/`), traduções (`po/`), dependências (`subprojects/`) e documentação (`docs/`).
- Adicionado [`docs/estrutura.md`](../estrutura.md), com mapa das pastas, regra para novos documentos e explicação dos artefatos locais ignorados.
- Movidos o plano, PDR, design, fontes e índice de skills para subpastas temáticas de `docs/`; atualizados links locais e instruções.
- Reunidos builds, caches e pacotes locais em `build/`, com arquivo de configurações antigas e destinos separados para perfis/builds futuros.
- Confirmado que o workflow GitHub Actions existente compila/verifica os perfis `development` e `default`.
- Atualizado o quadro de implementação para registrar o escopo e as limitações da preparação.

## Verificação e pendências

O estado Git estava limpo antes desta preparação. Os caminhos citados no README foram conferidos na árvore do repositório. A reorganização preservou as configurações Meson anteriores em `build/archive/`; elas apontavam para a cópia antiga `/home/thiago/Projects/termac` e não foram usadas como builds ativos. Não foi executado build nem suíte de testes nesta reorganização.

Esta cópia não tem remoto Git configurado. Para publicar, ainda é preciso criar ou escolher o destino no GitHub, configurar o remoto e enviar o histórico. A validação final no GNOME/Wayland também continua pendente conforme o plano vigente.
