# Stream SWBR

![Stream SWBR](resources/icon/stream-swbr-horizontal.png)

**Sua biblioteca de filmes e séries no Nintendo Switch.**

Cliente homebrew com interface em português, navegação por controle e reprodução de streams HTTPS fornecidos por complementos compatíveis com Stremio.

## Download e instalação

Baixe `StreamFin.nro` na página de [Releases](https://github.com/zDSiilva/streamfin26-switch/releases) e copie para `/switch/` no cartão SD. O arquivo mantém esse nome para compatibilidade com os atalhos existentes; o aplicativo aparece como **Stream SWBR** no hbmenu.

Para atualizar, feche o aplicativo e substitua o arquivo anterior no mesmo caminho. Biblioteca, histórico e preferências continuam em `config/StreamFin/`. O ícone de um atalho HOME previamente instalado é definido pelo próprio atalho.

## Recursos

- Header fixo com logo horizontal, configurações e busca.
- Categorias com até 15 títulos por fileira e **Ver todos** para acessar o catálogo carregado completo.
- Nomes e sinopses em português por padrão, sem conta ou token, via serviço público TMDB Addon / ElfHosted. Nem todas as obras ou episódios possuem tradução disponível.
- Cartões com títulos em duas linhas e estrela de favoritos.
- Biblioteca, continuar assistindo, filmes, séries, animes e categorias configuráveis.
- Seleção de fontes organizada, servidor preferido, filtros e preferências de áudio/legendas.
- `+` abre configurações na tela inicial; a explicação dos controles fica nas configurações.

## Complemento de vídeo

O aplicativo não inclui filmes ou séries. Configure seu complemento de streams HTTPS em Configurações ou no arquivo `/switch/streamfin-addon.txt`. Downloads por torrent não são suportados pelo player.

O serviço público de metadados é mantido por terceiros e pode ficar indisponível. Os nomes recebidos são armazenados no cartão SD para reduzir consultas; as primeiras cargas ainda dependem da rede. Tokens pessoais não são incluídos no código ou no aplicativo distribuído.

## Créditos e licença

Baseado em [Kirbosh/StreamFinEX](https://github.com/Kirbosh/StreamFinEX), derivado de [scamNscoot/StreamFin](https://github.com/scamNscoot/StreamFin) e [Switchfin](https://github.com/dragonflylee/switchfin). Identidade Stream SWBR e adaptações desta edição por zDSiilva. Créditos, avisos e licenças dos projetos originais e dependências são preservados.

Código sob [Apache License 2.0](LICENSE). Consulte [README-PT-BR.md](README-PT-BR.md) para detalhes das alterações e compilação.

Dados: [TMDB](https://www.themoviedb.org/), via [TMDB Addon / ElfHosted](https://tmdb.elfhosted.com/).

This product uses the TMDB API but is not endorsed or certified by TMDB.
