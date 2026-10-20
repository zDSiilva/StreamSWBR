# Stream SWBR

![Stream SWBR](resources/icon/stream-swbr-horizontal.png)

**Sua biblioteca de filmes e séries no Nintendo Switch.**

Cliente homebrew com interface em português, navegação por controle e reprodução de streams HTTPS fornecidos por complementos compatíveis com Stremio.

## Download e instalação

Baixe `Stream-SWBR-1.1.0.zip` em [Releases](https://github.com/zDSiilva/StreamSWBR/releases), extraia e copie a pasta `switch` para a raiz do cartão SD.

```text
switch/
└── Stream-SWBR-1.1.0/
    └── Stream-SWBR-1.1.0.nro
```

No HB Menu, selecione Stream SWBR, versão 1.1.0. Biblioteca, histórico e preferências continuam em `config/StreamFin/`. Atalhos HOME que apontam para um caminho antigo precisam manter esse caminho ou ser ajustados para o novo.

## Recursos

- Header fixo com logo horizontal, configurações e busca.
- Categorias com até 15 títulos por fileira e **Ver todos** para acessar o catálogo carregado completo.
- Nomes e sinopses em português por padrão, sem conta ou token, via serviço público TMDB Addon / ElfHosted. Nem todas as obras ou episódios possuem tradução disponível.
- Cartões com títulos em duas linhas e estrela de favoritos.
- Biblioteca, continuar assistindo, filmes, séries, animes e categorias configuráveis.
- Seleção de fontes organizada, servidor preferido, filtros e preferências de áudio/legendas.
- `+` abre configurações na tela inicial; a explicação dos controles fica nas configurações.

## Complementos de vídeo

O aplicativo não inclui filmes, séries ou endereços de vídeo predefinidos. Na primeira abertura, a tela de complementos aparece vazia. Você pode voltar e navegar pelos catálogos sem configurar vídeo.

Em Configurações → Complementos de vídeo, adicione endereços HTTP/HTTPS compatíveis com Stremio, dê nomes e selecione qual usar. Até 20 complementos ficam salvos; apenas o ativo busca vídeos. É possível renomear, alterar endereço e excluir. O complemento configurado na versão anterior é preservado na lista.

Você também pode importar um endereço pelo arquivo `/switch/streamfin-addon.txt`. O endereço é importado uma vez: o arquivo inalterado não desfaz uma seleção nem restaura um complemento excluído. Formatos `/manifest.json` e `/manifest/json` são reconhecidos. Isso não garante que qualquer serviço esteja disponível ou ofereça URLs HTTPS reproduzíveis; torrents não são suportados.

Para testar como primeira instalação sem apagar dados, feche o app, renomeie temporariamente `config/StreamFin/` para `config/StreamFin-backup/` e `/switch/streamfin-addon.txt` para `/switch/streamfin-addon.txt.bak`, se existirem. Escolha nomes de backup ainda não utilizados. Depois abra a versão 1.1.0. Não é necessário excluir o repositório do GitHub.

O serviço público de metadados é mantido por terceiros e pode ficar indisponível. Os nomes recebidos são armazenados no cartão SD para reduzir consultas; as primeiras cargas ainda dependem da rede. Tokens pessoais não são incluídos no código ou no aplicativo distribuído.

## Créditos e licença

Baseado em [Kirbosh/StreamFinEX](https://github.com/Kirbosh/StreamFinEX), derivado de [scamNscoot/StreamFin](https://github.com/scamNscoot/StreamFin) e [Switchfin](https://github.com/dragonflylee/switchfin). Identidade Stream SWBR e adaptações desta edição por Diguit. Créditos, avisos e licenças dos projetos originais e dependências são preservados.

Código sob [Apache License 2.0](LICENSE). Consulte [README-PT-BR.md](README-PT-BR.md) para detalhes das alterações e compilação.

Dados: [TMDB](https://www.themoviedb.org/), via [TMDB Addon / ElfHosted](https://tmdb.elfhosted.com/).

This product uses the TMDB API but is not endorsed or certified by TMDB.
