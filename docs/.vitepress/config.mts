import { defineConfig } from "vitepress";

export const SITE_URL = "https://muhammad-fiaz.github.io/zstd.zig";
export const SITE_NAME = "zstd.zig";
export const SITE_DESCRIPTION =
  "A complete native Zig implementation of Zstandard compression. No C bindings, no dependencies. Supports compression, decompression, streaming, and dictionary-based operations.";

export const GA_ID = "G-6BVYCRK57P";
export const GTM_ID = "GTM-P4M9T8ZR";
export const ADSENSE_CLIENT_ID = "ca-pub-2040560600290490";

export const KEYWORDS =
  "zig, zstd, zstandard, compression, decompression, native, pure-zig, streaming, dictionary, lossless, data-compression, binary";

export default defineConfig({
  lang: "en-US",
  title: SITE_NAME,
  description: SITE_DESCRIPTION,
  base: "/zstd.zig/",
  lastUpdated: true,
  cleanUrls: false,

  sitemap: {
    hostname: SITE_URL,
  },

  head: [
    ["meta", { name: "title", content: SITE_NAME }],
    ["meta", { name: "description", content: SITE_DESCRIPTION }],
    ["meta", { name: "keywords", content: KEYWORDS }],
    ["meta", { name: "author", content: "Muhammad Fiaz" }],
    ["meta", { name: "robots", content: "index, follow" }],
    ["meta", { name: "language", content: "English" }],
    ["meta", { name: "revisit-after", content: "7 days" }],
    ["meta", { name: "generator", content: "VitePress" }],

    ["meta", { property: "og:type", content: "website" }],
    ["meta", { property: "og:url", content: SITE_URL }],
    ["meta", { property: "og:title", content: SITE_NAME }],
    ["meta", { property: "og:description", content: SITE_DESCRIPTION }],
    [
      "meta",
      {
        property: "og:image",
        content: `${SITE_URL}/favicon.svg`,
      },
    ],
    [
      "meta",
      {
        property: "og:image:alt",
        content: "zstd.zig - Native Zig compression library",
      },
    ],
    ["meta", { property: "og:site_name", content: SITE_NAME }],
    ["meta", { property: "og:locale", content: "en_US" }],

    ["meta", { name: "twitter:card", content: "summary" }],
    ["meta", { name: "twitter:url", content: SITE_URL }],
    ["meta", { name: "twitter:title", content: SITE_NAME }],
    ["meta", { name: "twitter:description", content: SITE_DESCRIPTION }],
    [
      "meta",
      {
        name: "twitter:image",
        content: `${SITE_URL}/favicon.svg`,
      },
    ],
    [
      "meta",
      {
        name: "twitter:image:alt",
        content: "zstd.zig - Native Zig compression library",
      },
    ],
    ["meta", { name: "twitter:site", content: "@muhammadfiaz_" }],
    ["meta", { name: "twitter:creator", content: "@muhammadfiaz_" }],

    ["link", { rel: "canonical", href: SITE_URL }],

    ["link", { rel: "icon", type: "image/svg+xml", href: "/zstd.zig/favicon.svg" }],
    ["link", { rel: "icon", type: "image/png", sizes: "32x32", href: "/zstd.zig/favicon.svg" }],

    ["meta", { name: "theme-color", content: "#F7C948" }],
    ["meta", { name: "msapplication-TileColor", content: "#F7C948" }],

    [
      "script",
      {
        async: "",
        src: `https://www.googletagmanager.com/gtag/js?id=${GA_ID}`,
      },
    ],
    [
      "script",
      {},
      `window.dataLayer = window.dataLayer || [];
function gtag(){dataLayer.push(arguments);}
gtag('js', new Date());
gtag('config', '${GA_ID}');`,
    ],

    ...(GTM_ID
      ? ([
          [
            "script",
            {},
            `(function(w,d,s,l,i){w[l]=w[l]||[];w[l].push({'gtm.start': new Date().getTime(),event:'gtm.js'});var f=d.getElementsByTagName(s)[0], j=d.createElement(s), dl=l!='dataLayer'?'&l='+l:''; j.async=true; j.src='https://www.googletagmanager.com/gtm.js?id='+i+dl; f.parentNode.insertBefore(j,f);})(window,document,'script','dataLayer','${GTM_ID}');`,
          ],
          [
            "noscript",
            {},
            `<iframe src="https://www.googletagmanager.com/ns.html?id=${GTM_ID}" height="0" width="0" style="display:none;visibility:hidden"></iframe>`,
          ],
        ] as [string, Record<string, string>, string][])
      : []),

    [
      "script",
      {
        async: "",
        src: `https://pagead2.googlesyndication.com/pagead/js/adsbygoogle.js?client=${ADSENSE_CLIENT_ID}`,
        crossorigin: "anonymous",
      },
    ],
  ],

  ignoreDeadLinks: [/.*\.zig$/],

  transformPageData(pageData: any) {
    const pageTitle = pageData.title || SITE_NAME;
    const pageDescription = pageData.description || SITE_DESCRIPTION;
    const normalizedPath = pageData.relativePath
      .replace(/\.md$/, "")
      .replace(/(^|\/)index$/, "$1")
      .replace(/\/$/, "");
    const canonicalUrl =
      normalizedPath.length > 0
        ? `${SITE_URL}/${normalizedPath}`
        : SITE_URL;

    pageData.frontmatter.head ??= [];
    pageData.frontmatter.head.push(
      ["link", { rel: "canonical", href: canonicalUrl }],
      [
        "meta",
        {
          property: "og:title",
          content: `${pageTitle} | ${SITE_NAME}`,
        },
      ],
      ["meta", { property: "og:url", content: canonicalUrl }],
    );

    if (pageData.frontmatter.description) {
      pageData.frontmatter.head.push(
        [
          "meta",
          {
            property: "og:description",
            content: pageData.frontmatter.description,
          },
        ],
        [
          "meta",
          { name: "description", content: pageData.frontmatter.description },
        ],
      );
    }

    const isHome = pageData.relativePath === "index.md";
    const lastUpdated = pageData.lastUpdated
      ? new Date(pageData.lastUpdated).toISOString()
      : new Date().toISOString();

    const graph: any[] = [];

    if (isHome) {
      graph.push({
        "@type": "WebSite",
        name: SITE_NAME,
        url: SITE_URL,
        description: SITE_DESCRIPTION,
        author: {
          "@type": "Person",
          name: "Muhammad Fiaz",
          url: "https://github.com/muhammad-fiaz",
        },
      });
    }

    const authorSchema = {
      "@type": "Person",
      name: "Muhammad Fiaz",
      url: "https://muhammadfiaz.com",
      sameAs: [
        "https://github.com/muhammad-fiaz",
        "https://www.linkedin.com/in/muhammad-fiaz-",
        "https://x.com/muhammadfiaz_",
      ],
    };

    const primarySchema: Record<string, any> = {
      "@type": isHome ? "SoftwareApplication" : "TechArticle",
      name: isHome ? SITE_NAME : pageTitle,
      description: pageDescription,
      url: canonicalUrl,
      image: `${SITE_URL}/favicon.svg`,
      author: authorSchema,
      publisher: {
        "@type": "Organization",
        name: "zstd.zig",
        url: SITE_URL,
      },
    };

    if (isHome) {
      Object.assign(primarySchema, {
        applicationCategory: "DeveloperApplication",
        operatingSystem: "Cross-platform",
        programmingLanguage: "Zig",
        offers: {
          "@type": "Offer",
          price: "0",
          priceCurrency: "USD",
        },
        downloadUrl: "https://github.com/muhammad-fiaz/zstd.zig",
        license: "https://opensource.org/licenses/MIT",
        featureList: [
          "Pure Zig implementation",
          "One-shot compression and decompression",
          "Streaming compression and decompression",
          "Dictionary-based compression (CDict/DDict)",
          "Frame inspection (magic number, content size, compressed size)",
          "Cross-platform (Linux, Windows, macOS)",
          "Zero external dependencies",
          "Zig 0.17.0+ support",
        ],
      });
    } else {
      const pathParts = pageData.relativePath.split("/");
      const section =
        pathParts.length > 1
          ? pathParts[0].charAt(0).toUpperCase() + pathParts[0].slice(1)
          : "Documentation";

      Object.assign(primarySchema, {
        headline: pageTitle,
        articleSection: section,
        mainEntityOfPage: {
          "@type": "WebPage",
          "@id": canonicalUrl,
        },
        datePublished: "2026-01-01T00:00:00Z",
        dateModified: lastUpdated,
      });
    }
    graph.push(primarySchema);

    const breadcrumbs: any[] = [
      {
        "@type": "ListItem",
        position: 1,
        name: "Home",
        item: SITE_URL,
      },
    ];

    if (!isHome) {
      const pathParts = pageData.relativePath.replace(/\.md$/, "").split("/");
      let currentPath = SITE_URL;

      pathParts.forEach((part: string, index: number) => {
        currentPath += `/${part}`;
        const name = part
          .split("-")
          .map((s: string) => s.charAt(0).toUpperCase() + s.slice(1))
          .join(" ");

        breadcrumbs.push({
          "@type": "ListItem",
          position: index + 2,
          name: name,
          item:
            index === pathParts.length - 1 ? canonicalUrl : currentPath,
        });
      });
    }

    graph.push({
      "@type": "BreadcrumbList",
      itemListElement: breadcrumbs,
    });

    pageData.frontmatter.head.push([
      "script",
      { type: "application/ld+json" },
      JSON.stringify({
        "@context": "https://schema.org",
        "@graph": graph,
      }),
    ]);
  },

  themeConfig: {
    siteTitle: "zstd.zig",

    nav: [
      { text: "Home", link: "/" },
      { text: "Guide", link: "/guide/getting-started" },
      { text: "API", link: "/api/" },
      { text: "Examples", link: "/examples/" },
      {
        text: "Releases",
        link: "https://github.com/muhammad-fiaz/zstd.zig/releases",
      },
      {
        text: "Support",
        items: [
          {
            text: "Sponsor",
            link: "https://github.com/sponsors/muhammad-fiaz",
          },
          { text: "Donate", link: "https://pay.muhammadfiaz.com" },
        ],
      },
      { text: "GitHub", link: "https://github.com/muhammad-fiaz/zstd.zig" },
    ],

    sidebar: [
      {
        text: "Guide",
        items: [
          { text: "Getting Started", link: "/guide/getting-started" },
          { text: "Installation", link: "/guide/installation" },
          { text: "Compression", link: "/guide/compression" },
          { text: "Decompression", link: "/guide/decompression" },
          { text: "Streaming", link: "/guide/streaming" },
          { text: "Dictionaries", link: "/guide/dictionaries" },
        ],
      },
      {
        text: "API Reference",
        items: [
          { text: "Overview", link: "/api/" },
          { text: "CompressOptions", link: "/api/compress-options" },
          { text: "DecompressOptions", link: "/api/decompress-options" },
          { text: "Compressor", link: "/api/compressor" },
          { text: "Decompressor", link: "/api/decompressor" },
          { text: "StreamCompressor", link: "/api/stream-compressor" },
          { text: "StreamDecompressor", link: "/api/stream-decompressor" },
          { text: "CDict / DDict", link: "/api/dict" },
          { text: "Frame", link: "/api/frame" },
          { text: "CLevel", link: "/api/clevel" },
          { text: "Constants", link: "/api/constants" },
          { text: "Errors", link: "/api/errors" },
        ],
      },
      {
        text: "Examples",
        items: [
          { text: "Overview", link: "/examples/" },
          { text: "Basic Compression", link: "/examples/basic" },
          { text: "Streaming", link: "/examples/streaming" },
          { text: "Dictionary", link: "/examples/dictionary" },
          { text: "Frame Inspection", link: "/examples/frame" },
        ],
      },
    ],

    socialLinks: [
      { icon: "github", link: "https://github.com/muhammad-fiaz/zstd.zig" },
    ],

    footer: {
      message: "Released under the MIT License.",
      copyright: "Copyright © 2026 Muhammad Fiaz",
    },

    search: {
      provider: "local",
    },

    editLink: {
      pattern:
        "https://github.com/muhammad-fiaz/zstd.zig/edit/main/docs/:path",
      text: "Edit this page on GitHub",
    },

    lastUpdated: {
      text: "Last updated",
      formatOptions: {
        dateStyle: "medium",
        timeStyle: "short",
      },
    },
  },
});
