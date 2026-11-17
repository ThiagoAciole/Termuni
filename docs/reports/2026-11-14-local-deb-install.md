# Migração para distribuição `.deb`

Em 14/11/2026, o perfil `default` foi compilado para amd64, empacotado localmente como `thiago.aciole.termac` 0.1.0 e instalado via APT. O novo roteiro reproduzível é `python3 tools/build-deb.py`; o `.deb` gerado em `dist/` contém o executável e os arquivos de integração desktop.

O Meson instala `/usr/share/icons/hicolor/scalable/apps/thiago.aciole.Termac.svg`, gerado pelo preparador existente a partir de `data/icons/hicolor/scalable/apps/thiago.aciole.Termac.svg`. O desktop entry referencia `Icon=thiago.aciole.Termac` e `Exec=termac`. Binário, schema GSettings e serviço D-Bus também foram conferidos nos caminhos de sistema.

APT reportou `thiago.aciole.termac 0.1.0 amd64 installed`; `dpkg -S` confirmou que o ícone pertence ao pacote. Depois da migração do código para shell/PTY locais, o mesmo pacote foi reconstruído e reinstalado. O app `.Devel` do pacote antigo foi removido do escopo Flatpak do usuário com `--keep-ref` e sem `--delete-data`; não havia processo Termac ativo, e os dados pessoais do app foram preservados. A instalação `.deb` não foi publicada em repositório/release. Não houve lançamento visual do aplicativo nesta etapa.
