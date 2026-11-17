# Validação antiga de agentes no pacote Flatpak

Registro preservado após a decisão de distribuir o Termac apenas como `.deb`. Não descreve o produto atual.

## Repetição pelo Flatpak — 13/11/2026

O build GNOME 50/development com o bridge host foi exercitado em Xvfb próprio, cache de teste ligado ao mesmo caminho no host/sandbox e os CLIs reais já autenticados. **6/6 recebimentos multimodais passaram novamente**:

| Fluxo Flatpak → host | Claude Code 2.1.325 | Codex CLI 0.167.1 |
| --- | --- | --- |
| PNG com espaço/apóstrofo/acento | K7R4, quadrado vermelho/círculo azul | K7R4, quadrado vermelho/círculo azul |
| JPEG | M2X9, triângulo amarelo/retângulo roxo | M2X9, triângulo amarelo/retângulo roxo |
| Clipboard de imagem + Ctrl+V | V8C6, triângulo verde/retângulo laranja | V8C6, triângulo verde/retângulo laranja |

O prompt não continha os códigos e proibia ferramentas/leitura de arquivos. As respostas exibiram os códigos e as formas; não houve chamada de ferramenta nos diálogos. As grades observadas ficam nos arquivos locais `build/{claude,codex}-flatpak-{png,jpeg,clipboard}.txt`. O driver foi encerrado pelo comando `quit`, somente nas próprias janelas. Nenhum CLI pessoal foi atualizado.

Isso conclui a repetição funcional V05 no Flatpak/Xvfb. Gestos físicos/clipboard do compositor Wayland continuam como aceite separado.
