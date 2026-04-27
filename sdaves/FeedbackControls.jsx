import { motion, AnimatePresence } from 'framer-motion';
import { Settings } from 'lucide-react';
import { useState, useRef } from 'react';

const BLEND_MODES = [
    { label: 'Source Over', value: 'source-over' },
    { label: 'Source In', value: 'source-in' },
    { label: 'Source Out', value: 'source-out' },
    { label: 'Source Atop', value: 'source-atop' },
    { label: 'Destination Over', value: 'destination-over' },
    { label: 'Destination In', value: 'destination-in' },
    { label: 'Destination Out', value: 'destination-out' },
    { label: 'Destination Atop', value: 'destination-atop' },
    { label: 'Lighter (Add)', value: 'lighter' },
    { label: 'Copy', value: 'copy' },
    { label: 'XOR', value: 'xor' },
    { label: 'Multiply', value: 'multiply' },
    { label: 'Screen', value: 'screen' },
    { label: 'Overlay', value: 'overlay' },
    { label: 'Darken', value: 'darken' },
    { label: 'Lighten', value: 'lighten' },
    { label: 'Color Dodge', value: 'color-dodge' },
    { label: 'Color Burn', value: 'color-burn' },
    { label: 'Hard Light', value: 'hard-light' },
    { label: 'Soft Light', value: 'soft-light' },
    { label: 'Difference', value: 'difference' },
    { label: 'Exclusion', value: 'exclusion' },
    { label: 'Hue', value: 'hue' },
    { label: 'Saturation', value: 'saturation' },
    { label: 'Color', value: 'color' },
    { label: 'Luminosity', value: 'luminosity' },
];

export default function FeedbackControls({ settings, onChange, isVisible, showBlendMode, colorSettings, onColorSettingsChange }) {
    const [open, setOpen] = useState(false);
    const [expandColorCorrection, setExpandColorCorrection] = useState(false);
    const [expandPreset, setExpandPreset] = useState(false);
    const [expandFeedback, setExpandFeedback] = useState(false);
    const [expandCircular, setExpandCircular] = useState(false);
    const [expandEdge, setExpandEdge] = useState(false);

    return (
        <motion.div
            initial={{ opacity: 0, x: 100 }}
            animate={{ opacity: isVisible ? 1 : 0, x: isVisible ? 0 : 100, pointerEvents: isVisible ? 'auto' : 'none' }}
            transition={{ duration: 0.3 }}
            className="fixed right-8 top-8 z-50"
        >
            {/* Icon Button */}
            <button
                onClick={() => setOpen(o => !o)}
                title="Feedback Settings"
                className={`w-12 h-12 flex items-center justify-center rounded-2xl backdrop-blur-xl border text-white hover:bg-white/10 transition-colors shadow-2xl ${
                    open ? 'bg-pink-500/30 border-pink-400/40' : 'bg-black/40 border-white/10'
                }`}
            >
                <Settings className="w-5 h-5" />
            </button>

            {/* Dropdown Panel */}
            <AnimatePresence>
                {open && (
                    <motion.div
                        initial={{ opacity: 0, scale: 0.95, y: -8 }}
                        animate={{ opacity: 1, scale: 1, y: 0 }}
                        exit={{ opacity: 0, scale: 0.95, y: -8 }}
                        transition={{ duration: 0.15 }}
                        className="absolute top-14 right-0 bg-black/80 backdrop-blur-xl border border-white/10 rounded-2xl shadow-2xl overflow-hidden"
                    >
                        <div className="px-4 py-4 space-y-4 w-52 max-h-[75vh] overflow-y-auto scrollbar-thin scrollbar-thumb-white/30 scrollbar-track-white/5">

                            {/* Color Correction Section */}
                            {colorSettings && onColorSettingsChange && (
                                <div>
                                    <button
                                        onClick={() => setExpandColorCorrection(!expandColorCorrection)}
                                        className="flex items-center justify-between w-full text-xs text-white/50 font-semibold uppercase tracking-wider mb-3 hover:text-white/70 transition"
                                    >
                                        <span>Color Correction</span>
                                        <span className="text-xs">{expandColorCorrection ? '▼' : '▶'}</span>
                                    </button>
                                    {expandColorCorrection && (
                                        <div className="space-y-4 mb-3 pb-3 border-b border-white/10">
                                            {/* GAIN RGB */}
                                            <RgbGroup label="Gain" min={0.1} max={4} step={0.05}
                                                r={colorSettings.gainR ?? 1} g={colorSettings.gainG ?? 1} b={colorSettings.gainB ?? 1}
                                                onR={v => onColorSettingsChange({ ...colorSettings, gainR: v })}
                                                onG={v => onColorSettingsChange({ ...colorSettings, gainG: v })}
                                                onB={v => onColorSettingsChange({ ...colorSettings, gainB: v })}
                                            />

                                            {/* LIFT RGB */}
                                            <RgbGroup label="Lift" min={0} max={0.5} step={0.01}
                                                r={colorSettings.liftR ?? 0} g={colorSettings.liftG ?? 0} b={colorSettings.liftB ?? 0}
                                                onR={v => onColorSettingsChange({ ...colorSettings, liftR: v })}
                                                onG={v => onColorSettingsChange({ ...colorSettings, liftG: v })}
                                                onB={v => onColorSettingsChange({ ...colorSettings, liftB: v })}
                                            />

                                            {/* GAMMA RGB */}
                                            <RgbGroup label="Gamma" min={0.3} max={3} step={0.05}
                                                r={colorSettings.gammaR ?? 1} g={colorSettings.gammaG ?? 1} b={colorSettings.gammaB ?? 1}
                                                onR={v => onColorSettingsChange({ ...colorSettings, gammaR: v })}
                                                onG={v => onColorSettingsChange({ ...colorSettings, gammaG: v })}
                                                onB={v => onColorSettingsChange({ ...colorSettings, gammaB: v })}
                                            />

                                            <Slider label="Pre-Saturate" value={colorSettings.preSaturate ?? 1} min={0.5} max={4} step={0.05}
                                                onChange={v => onColorSettingsChange({ ...colorSettings, preSaturate: v })}
                                                displayValue={`x${(colorSettings.preSaturate ?? 1).toFixed(2)}`} />

                                            {/* LUT Loader */}
                                            <LutLoader
                                                lutName={colorSettings.lutName}
                                                onLutLoad={(lutData, name) => onColorSettingsChange({ ...colorSettings, lutData, lutName: name })}
                                                onLutClear={() => onColorSettingsChange({ ...colorSettings, lutData: null, lutName: null })}
                                            />
                                            {colorSettings.lutName && (
                                                <Slider label="LUT Stärke" value={colorSettings.lutStrength ?? 1} min={0} max={1} step={0.01}
                                                    onChange={v => onColorSettingsChange({ ...colorSettings, lutStrength: v })}
                                                    displayValue={`${Math.round((colorSettings.lutStrength ?? 1) * 100)}%`} />
                                            )}
                                        </div>
                                    )}
                                </div>
                            )}

                            {/* Preset Section (collapsible) */}
                            <div>
                                <button
                                    onClick={() => setExpandPreset(!expandPreset)}
                                    className="flex items-center justify-between w-full text-xs text-white/50 font-semibold uppercase tracking-wider mb-3 hover:text-white/70 transition"
                                >
                                    <span>Preset</span>
                                    <span className="text-xs">{expandPreset ? '▼' : '▶'}</span>
                                </button>
                                {expandPreset && (
                                    <div className="space-y-4 mb-3 pb-3 border-b border-white/10">
                                        <Slider label="Zoom (konstant)" value={settings.zoomBase ?? 0} min={-10} max={10} step={0.1}
                                            onChange={v => onChange({ ...settings, zoomBase: v })}
                                            displayValue={(settings.zoomBase ?? 0) > 0 ? `+${(settings.zoomBase ?? 0).toFixed(1)}` : (settings.zoomBase ?? 0).toFixed(1)} />
                                        <Slider label="Rotation Speed" value={settings.rotationSpeed} min={0} max={2} step={0.01}
                                            onChange={v => onChange({ ...settings, rotationSpeed: v })}
                                            displayValue={`x${settings.rotationSpeed.toFixed(2)}`} />
                                        <Slider label="Original" value={settings.cameraBlend ?? 0.5} min={0} max={1} step={0.01}
                                            onChange={v => onChange({ ...settings, cameraBlend: v })}
                                            displayValue={`${Math.round((settings.cameraBlend ?? 0.5) * 100)}%`} />
                                        <Slider label="Brightness" value={settings.brightness ?? 1} min={0} max={2} step={0.01}
                                            onChange={v => onChange({ ...settings, brightness: v })}
                                            displayValue={`${Math.round((settings.brightness ?? 1) * 100)}%`} />
                                        <Slider label="Contrast" value={settings.contrast ?? 1} min={0} max={3} step={0.01}
                                            onChange={v => onChange({ ...settings, contrast: v })}
                                            displayValue={`${Math.round((settings.contrast ?? 1) * 100)}%`} />
                                        <Slider label="Saturation" value={settings.saturation ?? 1} min={0} max={3} step={0.01}
                                            onChange={v => onChange({ ...settings, saturation: v })}
                                            displayValue={`${Math.round((settings.saturation ?? 1) * 100)}%`} />
                                    </div>
                                )}
                            </div>

                            {/* Feedback Effects Section (collapsible) */}
                            <div>
                                <button
                                    onClick={() => setExpandFeedback(!expandFeedback)}
                                    className="flex items-center justify-between w-full text-xs text-white/50 font-semibold uppercase tracking-wider mb-3 hover:text-white/70 transition"
                                >
                                    <span>Feedback Effects</span>
                                    <span className="text-xs">{expandFeedback ? '▼' : '▶'}</span>
                                </button>
                                {expandFeedback && (
                                    <div className="space-y-4 mb-3 pb-3 border-b border-white/10">
                                        <Slider label="Strength" value={1 - (settings.feedbackStrength ?? 0.55)} min={0} max={1} step={0.01}
                                            onChange={v => onChange({ ...settings, feedbackStrength: 1 - v })} />
                                        <Slider label="Feedback Brightness" value={settings.feedbackBrightness ?? 1} min={0} max={2} step={0.01}
                                            onChange={v => onChange({ ...settings, feedbackBrightness: v })}
                                            displayValue={`${Math.round((settings.feedbackBrightness ?? 1) * 100)}%`} />
                                        <Slider label="Feedback Contrast" value={settings.feedbackContrast ?? 1} min={0} max={3} step={0.01}
                                            onChange={v => onChange({ ...settings, feedbackContrast: v })}
                                            displayValue={`${Math.round((settings.feedbackContrast ?? 1) * 100)}%`} />
                                        <Slider label="Feedback Saturation" value={settings.feedbackSaturation ?? 1} min={0} max={3} step={0.01}
                                            onChange={v => onChange({ ...settings, feedbackSaturation: v })}
                                            displayValue={`${Math.round((settings.feedbackSaturation ?? 1) * 100)}%`} />
                                        <Slider label="Feedback Gamma" value={settings.feedbackGamma ?? 1} min={0.1} max={3} step={0.01}
                                            onChange={v => onChange({ ...settings, feedbackGamma: v })}
                                            displayValue={`${(settings.feedbackGamma ?? 1).toFixed(2)}`} />
                                    </div>
                                )}
                            </div>

                            {/* Circular Motion (collapsible) */}
                            <div>
                                <button
                                    onClick={() => setExpandCircular(!expandCircular)}
                                    className="flex items-center justify-between w-full text-xs text-white/50 font-semibold uppercase tracking-wider mb-3 hover:text-white/70 transition"
                                >
                                    <span>Circular Motion</span>
                                    <span className="text-xs">{expandCircular ? '▼' : '▶'}</span>
                                </button>
                                {expandCircular && (
                                    <div className="space-y-4 mb-3 pb-3 border-b border-white/10">
                                        <Slider label="Diameter" value={settings.circularMotionDiameter ?? 0} min={0} max={300} step={1}
                                            onChange={v => onChange({ ...settings, circularMotionDiameter: v })}
                                            displayValue={`${Math.round(settings.circularMotionDiameter ?? 0)}px`} />
                                        <Slider label="Depth" value={settings.circularMotionDepth ?? 0} min={0} max={1} step={0.01}
                                            onChange={v => onChange({ ...settings, circularMotionDepth: v })}
                                            displayValue={`${Math.round((settings.circularMotionDepth ?? 0) * 100)}%`} />
                                        <Slider label="Speed" value={settings.circularMotionSpeed ?? 1} min={0} max={5} step={0.01}
                                            onChange={v => onChange({ ...settings, circularMotionSpeed: v })}
                                            displayValue={`x${(settings.circularMotionSpeed ?? 1).toFixed(2)}`} />
                                    </div>
                                )}
                            </div>

                            {/* Edge Handling (collapsible) */}
                            <div>
                                <button
                                    onClick={() => setExpandEdge(!expandEdge)}
                                    className="flex items-center justify-between w-full text-xs text-white/50 font-semibold uppercase tracking-wider mb-3 hover:text-white/70 transition"
                                >
                                    <span>Edge Handling</span>
                                    <span className="text-xs">{expandEdge ? '▼' : '▶'}</span>
                                </button>
                                {expandEdge && (
                                    <div className="space-y-3 mb-3 pb-3 border-b border-white/10">
                                        <div>
                                            <div className="text-xs text-white/50 mb-1">Feedback Edges</div>
                                            <select
                                                value={settings.feedbackWrapMode ?? 'none'}
                                                onChange={e => onChange({ ...settings, feedbackWrapMode: e.target.value })}
                                                className="w-full bg-white/10 text-white text-xs rounded px-2 py-1.5 border border-white/10 cursor-pointer"
                                            >
                                                <option value="none" className="bg-gray-900">Black</option>
                                                <option value="repeat" className="bg-gray-900">Repeat</option>
                                                <option value="tile" className="bg-gray-900">Tile</option>
                                                <option value="mirror" className="bg-gray-900">Mirror</option>
                                                <option value="mirrorrepeat" className="bg-gray-900">Mirror + Tile</option>
                                            </select>
                                        </div>
                                        <div>
                                            <div className="text-xs text-white/50 mb-1">Geometry Edges</div>
                                            <select
                                                value={settings.geometryWrapMode ?? 'none'}
                                                onChange={e => onChange({ ...settings, geometryWrapMode: e.target.value })}
                                                className="w-full bg-white/10 text-white text-xs rounded px-2 py-1.5 border border-white/10 cursor-pointer"
                                            >
                                                <option value="none" className="bg-gray-900">Black</option>
                                                <option value="repeat" className="bg-gray-900">Repeat</option>
                                                <option value="tile" className="bg-gray-900">Tile</option>
                                                <option value="mirror" className="bg-gray-900">Mirror</option>
                                                <option value="mirrorrepeat" className="bg-gray-900">Mirror + Tile</option>
                                            </select>
                                        </div>
                                    </div>
                                )}
                            </div>

                            {showBlendMode && (
                                <div>
                                    <div className="text-xs text-white/50 mb-1">Mode</div>
                                    <select
                                        value={settings.blendMode ?? 'source-over'}
                                        onChange={e => onChange({ ...settings, blendMode: e.target.value })}
                                        className="w-full bg-white/10 text-white text-xs rounded px-2 py-1.5 border border-white/10 cursor-pointer"
                                    >
                                        {BLEND_MODES.map(m => (
                                            <option key={m.value} value={m.value} className="bg-gray-900">{m.label}</option>
                                        ))}
                                    </select>
                                </div>
                            )}
                        </div>
                    </motion.div>
                )}
            </AnimatePresence>
        </motion.div>
    );
}

function Slider({ label, value, min, max, step, onChange, displayValue, accent = 'accent-pink-400' }) {
    const [editing, setEditing] = useState(false);
    const [inputVal, setInputVal] = useState('');

    const startEdit = () => {
        setInputVal(String(value));
        setEditing(true);
    };

    const commitEdit = () => {
        const parsed = parseFloat(inputVal);
        if (!isNaN(parsed)) {
            onChange(Math.min(max, Math.max(min, parsed)));
        }
        setEditing(false);
    };

    return (
        <div>
            <div className="flex justify-between text-xs text-white/50 mb-1">
                <span>{label}</span>
                {editing ? (
                    <input
                        type="text"
                        inputMode="decimal"
                        value={inputVal}
                        onChange={e => setInputVal(e.target.value)}
                        onBlur={commitEdit}
                        onKeyDown={e => { if (e.key === 'Enter') commitEdit(); if (e.key === 'Escape') setEditing(false); }}
                        autoFocus
                        className="w-16 text-right bg-white/10 text-white text-xs px-1 rounded border border-white/20 outline-none focus:border-pink-400"
                    />
                ) : (
                    <span onClick={startEdit} className="cursor-pointer hover:text-white transition">{displayValue ?? value.toFixed(2)}</span>
                )}
            </div>
            <input
                type="range"
                min={min} max={max} step={step}
                value={value}
                onChange={e => onChange(parseFloat(e.target.value))}
                className={`w-full h-1 ${accent} cursor-pointer`}
            />
        </div>
    );
}

function RgbGroup({ label, min, max, step, r, g, b, onR, onG, onB }) {
    return (
        <div>
            <div className="text-xs text-white/40 mb-1.5">{label}</div>
            <div className="pl-2 border-l border-white/10 space-y-2">
                <Slider label="R" value={r} min={min} max={max} step={step}
                    onChange={onR} displayValue={r.toFixed(2)} accent="accent-red-400" />
                <Slider label="G" value={g} min={min} max={max} step={step}
                    onChange={onG} displayValue={g.toFixed(2)} accent="accent-green-400" />
                <Slider label="B" value={b} min={min} max={max} step={step}
                    onChange={onB} displayValue={b.toFixed(2)} accent="accent-blue-400" />
            </div>
        </div>
    );
}

function LutLoader({ lutName, onLutLoad, onLutClear }) {
    const inputRef = useRef(null);

    const handleFile = (e) => {
        const file = e.target.files[0];
        if (!file) return;
        const reader = new FileReader();
        reader.onload = (ev) => {
            const text = ev.target.result;
            const lutData = parseCubeLut(text);
            if (lutData) onLutLoad(lutData, file.name);
            else alert('Ungültige .cube LUT-Datei');
        };
        reader.readAsText(file);
        e.target.value = '';
    };

    return (
        <div>
            <div className="text-xs text-white/40 mb-1.5">LUT (.cube)</div>
            <div className="flex items-center gap-2">
                <button
                    onClick={() => inputRef.current?.click()}
                    className="flex-1 text-xs px-2 py-1.5 rounded border border-white/10 bg-white/5 text-white/60 hover:bg-white/10 hover:text-white transition-colors text-left truncate"
                >
                    {lutName ? lutName : 'LUT laden…'}
                </button>
                {lutName && (
                    <button onClick={onLutClear} className="text-white/30 hover:text-red-400 transition-colors text-xs px-1">✕</button>
                )}
            </div>
            <input ref={inputRef} type="file" accept=".cube" className="hidden" onChange={handleFile} />
        </div>
    );
}

function parseCubeLut(text) {
    const lines = text.split('\n').map(l => l.trim()).filter(l => l && !l.startsWith('#'));
    let size = 0;
    const data = [];
    for (const line of lines) {
        if (line.startsWith('LUT_3D_SIZE')) {
            size = parseInt(line.split(/\s+/)[1]);
        } else if (/^[\d.eE+\-]+\s+[\d.eE+\-]+\s+[\d.eE+\-]+$/.test(line)) {
            const [r, g, b] = line.split(/\s+/).map(parseFloat);
            data.push(r, g, b);
        }
    }
    if (!size || data.length !== size * size * size * 3) return null;
    return { size, data: new Float32Array(data) };
}