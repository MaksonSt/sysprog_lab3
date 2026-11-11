import * as d3 from "d3";
import { colorOf } from "./categories.js";

const DURATION = 300;

function countNodes(data) {
    return 1 + (data.children || []).reduce((sum, c) => sum + countNodes(c), 0);
}

function countHidden(d) {
    return d._children ? d._children.reduce((sum, c) => sum + countNodes(c.data), 0) : 0;
}

function textMatches(data, query) {
    return data.kind.toLowerCase().includes(query) || (data.label || "").toLowerCase().includes(query);
}

function countMatches(data, query) {
    const self = textMatches(data, query) ? 1 : 0;
    return self + (data.children || []).reduce((sum, c) => sum + countMatches(c, query), 0);
}

export function createTree(svgElement, { onNodeClick, onHover }) {
    const svg = d3.select(svgElement);
    svg.selectAll("*").remove();
    const scene = svg.append("g");
    const linkLayer = scene.append("g");
    const nodeLayer = scene.append("g");

    const layout = d3.tree().nodeSize([26, 200]);
    const linkPath = d3.linkHorizontal().x((d) => d.y).y((d) => d.x);
    const zoom = d3.zoom().scaleExtent([0.1, 3]).on("zoom", (e) => scene.attr("transform", e.transform));
    svg.call(zoom).on("dblclick.zoom", null);

    let root = null;
    let activeLine = null;
    let query = "";

    const animate = (sel, ms) => (ms > 0 ? sel.transition().duration(ms) : sel);

    function markNodes(selection) {
        selection
            .classed("active", (d) => activeLine !== null && d.data.line === activeLine)
            .classed("found", (d) => query !== "" && textMatches(d.data, query));
    }

    function update(source, ms = DURATION) {
        layout(root);
        const nodes = root.descendants();

        const node = nodeLayer.selectAll("g.node").data(nodes, (d) => d.id);
        const enter = node.enter()
            .append("g")
            .attr("class", "node")
            .attr("transform", `translate(${source.y0},${source.x0})`)
            .attr("opacity", 0)
            .on("click", (event, d) => toggle(d))
            .on("mouseenter mousemove", (event, d) => onHover(d, event))
            .on("mouseleave", () => onHover(null));

        enter.append("circle").attr("r", 6);
        const label = enter.append("text").attr("dy", "0.32em").attr("x", 11);
        label.append("tspan").attr("class", "kind");
        label.append("tspan").attr("class", "label");
        label.append("tspan").attr("class", "hidden-count");

        const merged = enter.merge(node);
        animate(merged, ms).attr("transform", (d) => `translate(${d.y},${d.x})`).attr("opacity", 1);
        merged.select("circle")
            .attr("stroke", (d) => colorOf(d.data.kind))
            .attr("fill", (d) => (d._children ? colorOf(d.data.kind) : "var(--panel)"));
        merged.select(".kind").text((d) => d.data.kind).attr("fill", (d) => colorOf(d.data.kind));
        merged.select(".label").text((d) => (d.data.label ? " " + d.data.label : ""));
        merged.select(".hidden-count").text((d) => (d._children ? `  +${countHidden(d)}` : ""));
        markNodes(merged);

        animate(node.exit(), ms)
            .attr("transform", `translate(${source.y},${source.x})`)
            .attr("opacity", 0)
            .remove();

        const collapsed = (point) => linkPath({ source: point, target: point });
        const link = linkLayer.selectAll("path.link").data(root.links(), (d) => d.target.id);
        const linkEnter = link.enter()
            .append("path")
            .attr("class", "link")
            .attr("d", () => collapsed({ x: source.x0, y: source.y0 }));
        animate(linkEnter.merge(link), ms).attr("d", linkPath);
        animate(link.exit(), ms).attr("d", () => collapsed({ x: source.x, y: source.y })).remove();

        for (const d of nodes) {
            d.x0 = d.x;
            d.y0 = d.y;
        }
    }

    function toggle(d) {
        onNodeClick(d.data);
        if (!d.children && !d._children) return;
        [d.children, d._children] = [d._children, d.children];
        update(d);
    }

    function fit(ms = DURATION) {
        const box = scene.node().getBBox();
        const { clientWidth: width, clientHeight: height } = svgElement;
        if (!box.width || !width || !height) return;
        const pad = 30;
        const scale = Math.max(0.6, Math.min(1.1, (width - pad * 2) / box.width, (height - pad * 2) / box.height));
        const tx = box.width * scale < width - pad * 2 ? (width - box.width * scale) / 2 - box.x * scale : pad - box.x * scale;
        const ty = box.height * scale < height - pad * 2 ? (height - box.height * scale) / 2 - box.y * scale : pad - box.y * scale;
        svg.transition().duration(ms).call(zoom.transform, d3.zoomIdentity.translate(tx, ty).scale(scale));
    }

    return {
        setData(ast) {
            root = d3.hierarchy(ast);
            let id = 0;
            root.each((d) => { d.id = id++; });
            root.x0 = root.y0 = 0;
            nodeLayer.selectAll("*").remove();
            linkLayer.selectAll("*").remove();
            update(root, 0);
            fit(0);
            return { nodes: root.descendants().length, depth: root.height };
        },

        highlight(line) {
            activeLine = line;
            markNodes(nodeLayer.selectAll("g.node"));
        },

        search(text) {
            query = text.trim().toLowerCase();
            markNodes(nodeLayer.selectAll("g.node"));
            return root && query ? countMatches(root.data, query) : 0;
        },

        setExpanded(expanded) {
            if (!root) return;
            root.each((d) => {
                const kids = d.children || d._children;
                if (!kids) return;
                const open = expanded || d === root;
                d.children = open ? kids : null;
                d._children = open ? null : kids;
            });
            update(root);
            setTimeout(() => fit(), DURATION + 20);
        },

        fit,

        destroy() {
            svg.on(".zoom", null);
            svg.interrupt().selectAll("*").remove();
        },
    };
}
