import DefaultTheme from "vitepress/theme";
import { useData } from "vitepress";
import type { Theme } from "vitepress";
import "./custom.css";

export default {
  extends: DefaultTheme,
  Layout() {
    return h(DefaultTheme.Layout, null, {
      "doc-before": () => h(Breadcrumbs),
    });
  },
} satisfies Theme;

import { h, defineComponent } from "vue";

const Breadcrumbs = defineComponent({
  name: "Breadcrumbs",
  setup() {
    const { page } = useData();
    return () => {
      const path = page.value.relativePath.replace(/\.md$/, "").replace(/\/index$/, "");
      if (!path || path === "index") return null;
      const parts = path.split("/");
      const crumbs = parts.map((part, i) => ({
        text: part
          .replace(/-/g, " ")
          .replace(/\b\w/g, (c) => c.toUpperCase()),
        link: "/" + parts.slice(0, i + 1).join("/") + "/",
        isLast: i === parts.length - 1,
      }));
      return h("nav", { class: "vp-breadcrumb" }, [
        h("span", { class: "breadcrumb-item" }, [
          h("a", { href: "/zstd.zig/" }, "Home"),
        ]),
        ...crumbs.map((crumb) =>
          h("span", { class: "breadcrumb-item" }, [
            h("span", { class: "breadcrumb-separator" }, " > "),
            crumb.isLast
              ? h("span", { class: "breadcrumb-current" }, crumb.text)
              : h("a", { href: "/zstd.zig" + crumb.link }, crumb.text),
          ])
        ),
      ]);
    };
  },
});
