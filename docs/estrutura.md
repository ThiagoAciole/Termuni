# Estrutura do repositório

O repositório separa o código executável, recursos do aplicativo, validação, ferramentas e documentação. Mantenha os arquivos próximos da área a que pertencem e preserve os arquivos de entrada do Meson na raiz.

```text
Termuni/
├── .github/workflows/   CI do GitHub Actions
├── assets/              Artes originais usadas pela interface
├── data/                Ícones, schema, serviço D-Bus e metadados
├── docs/
│   ├── architecture/      Decisões de produto e arquitetura
│   ├── design/            Regras visuais e referências
│   ├── references/        Fontes técnicas e índice de skills
│   ├── PLANO_IMPLEMENTACAO.md Estado e próximos aceites
│   ├── checklists/        Aceites manuais e revisão visual
│   ├── history/           Registros antigos e backups preservados
│   ├── reports/           Evidências datadas de implementação/validação
│   └── superpowers/plans/  Planos dos incrementos executados
├── po/                  Traduções gettext e template
├── src/                 Aplicativo C e interface GTK
├── subprojects/         Wrap e integração Meson das dependências
├── tests/               Testes, fixtures e drivers opt-in
└── tools/               Scripts de desenvolvimento, CI e empacotamento
```

## Arquivos mantidos na raiz

`README.md` e `AGENTS.md` ficam na raiz como entrada do GitHub e instruções para agentes. Os documentos de planejamento e referência estão em `docs/`, agrupados por assunto. Arquivos Meson e `COPYING` também permanecem na raiz por convenção das ferramentas.

## Builds e artefatos locais

Todos os builds, caches e pacotes locais ficam sob `build/`:

```text
build/
├── development/       Perfil development
├── default/           Perfil instalável default
├── package/           Build de release para o .deb
├── ci/                Builds locais equivalentes à CI
├── diagnostics/       Builds especiais, como Valgrind sem SIMD
├── packages/          Pacotes .deb produzidos
├── cache/             Cache do Flatpak Builder
└── archive/           Diretórios antigos preservados como arquivo
```

As pastas antigas de build foram preservadas em `build/archive/`. Elas referenciam a cópia anterior `/home/thiago/Projects/termac` e não devem ser tratadas como builds ativos. Os builds atuais serão criados sob demanda pelos comandos desta documentação.

## Onde colocar novos arquivos

- Evidência de uma execução: `docs/reports/AAAA-MM-DD-assunto.md` e adicione uma entrada em [Índice da documentação](README.md).
- Procedimento de validação manual: `docs/checklists/`.
- Plano de implementação datado: `docs/superpowers/plans/`.
- Documentos substituídos, snapshots ou backups que precisam ser preservados: `docs/history/`.
- Testes reutilizáveis e fixtures sintéticas: `tests/`; dados de teste não devem conter credenciais pessoais.
- Scripts de suporte ao desenvolvimento: `tools/`, com instruções em [`tools/README.md`](../tools/README.md).

## Diretórios locais ignorados pelo Git

`build/`, `.agents/`, `.claude/`, `.superpowers/` e caches Python são locais ou gerados. Podem conter configurações e evidências de trabalho; estar no `.gitignore` não significa que devam ser apagados. Não adicione builds, pacotes gerados ou caches ao commit.

Confira o [`.gitignore`](../.gitignore) antes de alterar exclusões e use [Desenvolvimento](development.md) para escolher os diretórios de build e os comandos Meson.
