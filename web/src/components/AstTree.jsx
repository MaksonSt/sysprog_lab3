import { useEffect, useRef, useState } from "react";
import { CATEGORIES } from "../categories.js";
import { createTree } from "../tree.js";

export default function AstTree({ ast, activeLine, onNodeClick }) {
    const svgRef = useRef(null);
    const wrapRef = useRef(null);
    const treeRef = useRef(null);
    const clickRef = useRef(onNodeClick);
    clickRef.current = onNodeClick;

    const [stats, setStats] = useState(null);
    const [query, setQuery] = useState("");
    const [found, setFound] = useState(0);
    const [tooltip, setTooltip] = useState(null);

    // d3-дерево створюється один раз на весь час життя компонента
    useEffect(() => {
        const tree = createTree(svgRef.current, {
            onNodeClick: (data) => clickRef.current(data),
            onHover: (d, event) => {
                if (!d) return setTooltip(null);
                const rect = wrapRef.current.getBoundingClientRect();
                setTooltip({
                    x: event.clientX - rect.left + 14,
                    y: event.clientY - rect.top + 14,
                    kind: d.data.kind,
                    label: d.data.label,
                    line: d.data.line,
                    kids: (d.children || d._children || []).length,
                });
            },
        });
        treeRef.current = tree;
        const onResize = () => tree.fit(0);
        window.addEventListener("resize", onResize);
        return () => {
            window.removeEventListener("resize", onResize);
            tree.destroy();
        };
    }, []);

    useEffect(() => {
        if (!ast) return;
        setStats(treeRef.current.setData(ast));
    }, [ast]);

    useEffect(() => {
        setFound(treeRef.current.search(query));
    }, [ast, query]);

    useEffect(() => {
        treeRef.current.highlight(activeLine);
    }, [ast, activeLine]);

    return (
        <section className="panel tree-panel">
            <div className="panel-head">
                <h2>AST</h2>
                <div className="actions">
                    <input
                        className="search"
                        type="search"
                        placeholder="Пошук: For, cout, n…"
                        value={query}
                        onChange={(e) => setQuery(e.target.value)}
                    />
                    {query.trim() && <span className="muted">знайдено: {found}</span>}
                    <button className="button small" onClick={() => treeRef.current.setExpanded(true)}>Розгорнути</button>
                    <button className="button small" onClick={() => treeRef.current.setExpanded(false)}>Згорнути</button>
                    <button className="button small" onClick={() => treeRef.current.fit()}>Вписати</button>
                </div>
            </div>

            <div className="tree-wrap" ref={wrapRef}>
                <svg ref={svgRef} className="tree" />
                {tooltip && (
                    <div className="tooltip" style={{ left: tooltip.x, top: tooltip.y }}>
                        <b>{tooltip.kind}</b>
                        {tooltip.label && <> · {tooltip.label}</>}
                        {" · "}рядок {tooltip.line}
                        {tooltip.kids > 0 && <> · дітей: {tooltip.kids}</>}
                    </div>
                )}
            </div>

            <div className="tree-foot">
                <div className="legend">
                    {CATEGORIES.map((c) => (
                        <span key={c.name} className="legend-item">
                            <span className="legend-dot" style={{ background: c.color }} />
                            {c.name}
                        </span>
                    ))}
                </div>
                {stats && <span className="muted">Вузлів: {stats.nodes} · Глибина: {stats.depth}</span>}
            </div>
        </section>
    );
}
