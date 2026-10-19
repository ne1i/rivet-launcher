# Legal notice and disclaimer

Rivet is an independent, non-commercial open-source utility. It configures and
launches programs that are already on your computer. It is **not** affiliated
with, endorsed by, sponsored by, or approved by Valve Corporation, Steam,
Epic Games, OnlineFix, FreeTP, EOSFix, any game publisher, or any other
third-party service.

## No game content is hosted or distributed

This repository does not contain and does not distribute:

- game files, game assets, game keys, accounts or licenses;
- cracks, patches, or fixes for specific commercial games;
- a game download source, download links, or a catalog of pirated content.

The optional download browser is a generic aria2c front-end. It only contacts a
JSON source URL that **you** enter, and Rivet does not bundle, select, endorse
or operate any such source. The source and the files it points to are entirely
your responsibility.

Small third-party compatibility libraries are included solely so that existing
fix files can be used (`Newtonsoft.Json.dll`, `ftpPath32.dll`,
`ftpPath64.dll`). See `THIRD_PARTY.md` for provenance and licensing.

## Your responsibility

You are responsible for everything you do with this software. In particular:

- **Own the games you run.** You must hold a valid licence for every game,
  and you must not use Rivet to obtain or play games you have not legally
  acquired.
- **Copyright and technical protection measures.** In the European Union and
  elsewhere, circumventing technical protection measures and distributing or
  using unauthorised copies of copyrighted works can be unlawful. Online fixes,
  Steam emulation and similar techniques may fall under those rules in your
  country. Do not use Rivet if you do not have the right to do so.
- **Contracts and terms of service.** Using third-party fixes with Steam or
  other platforms may violate their terms of service and can lead to account
  restrictions. You accept that risk.
- **Local law.** Laws differ between EU member states and other countries.
  Nothing in this repository is legal advice, and no disclaimer can make an
  otherwise unlawful act lawful.

If you are unsure whether a particular use is legal where you live, consult a
qualified lawyer before using the software.

## No warranty and limitation of liability

Rivet is free software licensed under the GNU Affero General Public License
v3.0. It is provided **“as is”**, without warranty of any kind, express or
implied, including fitness for a particular purpose, title and
non-infringement.

To the maximum extent permitted by applicable law, the authors and
contributors are not liable for any direct, indirect, incidental, special,
exemplary or consequential damages, data loss, account bans, hardware damage,
or legal consequences arising from the use of, or inability to use, this
software.

Nothing in this notice excludes or limits liability that cannot be excluded or
limited under applicable law, including liability for intent, gross negligence,
or mandatory consumer-protection rights. If you are a consumer in the EU, your
statutory rights are unaffected; Rivet is provided free of charge and no paid
service or commercial guarantee is offered.

## Privacy

Rivet has no accounts, telemetry, analytics or advertising. Configuration is
stored locally under `$XDG_CONFIG_HOME/rivet/config.json`; artwork and Proton
builds live under `$XDG_DATA_HOME/rivet`. Network requests occur only when you
use a feature that needs them:

- the GitHub API, to list GE-Proton releases;
- Steam's cover CDN, to download game covers;
- a Hydra/OnlineFix source URL that you configure, to list downloads;
- the download URLs contained in that source, through `aria2c`;
- the links you explicitly open (Steam, ProtonDB, SteamDB, GitHub).

Those third-party servers receive your IP address and normal request metadata
under their own policies. Rivet does not send your library, game paths or
configuration to any server.

## Copyright complaints and takedown

If you are a rights holder and believe that material in this repository
infringes your rights, please open an issue on the repository or contact the
maintainer through GitHub, including the exact URL and the basis of your
request. Valid requests will be reviewed and the material will be removed or
replaced where appropriate. Do not include personal or confidential information
in a public issue.

## Severability

If any part of this notice is found unenforceable, the remaining parts remain
in effect. This notice is not legal advice.
