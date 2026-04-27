let _gammaFilterSvg = null;
let _lastGamma = 1;

function ensureGammaFilter(gamma) {
    if (gamma === 1 && !_gammaFilterSvg) return;
    const exp = 1 / Math.max(0.01, gamma);
    if (_gammaFilterSvg && Math.abs(exp - _lastGamma) < 0.001) return;
    _lastGamma = exp;
    if (!_gammaFilterSvg) {
        const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
        svg.setAttribute('width', '0');
        svg.setAttribute('height', '0');
        svg.style.position = 'absolute';
        const defs = document.createElementNS('http://www.w3.org/2000/svg', 'defs');
        const filter = document.createElementNS('http://www.w3.org/2000/svg', 'filter');
        filter.setAttribute('id', 'feedbackGamma');
        const comp = document.createElementNS('http://www.w3.org/2000/svg', 'feComponentTransfer');
        ['R', 'G', 'B'].forEach(ch => {
            const fn = document.createElementNS('http://www.w3.org/2000/svg', `feFunc${ch}`);
            fn.setAttribute('type', 'gamma');
            fn.setAttribute('amplitude', '1');
            fn.setAttribute('exponent', String(exp));
            fn.setAttribute('offset', '0');
            comp.appendChild(fn);
        });
        filter.appendChild(comp);
        defs.appendChild(filter);
        svg.appendChild(defs);
        document.body.appendChild(svg);
        _gammaFilterSvg = svg;
    } else {
        const funcs = _gammaFilterSvg.querySelectorAll('feComponentTransfer > *');
        funcs.forEach(fn => fn.setAttribute('exponent', String(exp)));
    }
}

/**
 * drawFeedback
 *
 * Draws the previous frame onto cCtx with zoom/rotation and optional edge-wrapping.
 * Supports beat-triggered rotation increments.
 *
 * For wrap modes we apply the zoom+rotation transform first, then tile ±1 around
 * the origin — that's at most 9 drawImage calls and always covers any visible gaps.
 */
export function drawFeedback(cCtx, img, w, h, feedbackWrapMode, zoom, angle, s, beatRotationIncrement = 0) {
    const alpha      = 1 - (s.feedbackStrength ?? 0.4);
    const blendMode  = s.blendMode          ?? 'screen';
    const brightness = s.feedbackBrightness ?? 1;
    const contrast   = s.feedbackContrast   ?? 1;
    const saturate   = s.feedbackSaturation ?? 1;
    const gamma      = s.feedbackGamma      ?? 1;

    // Ensure SVG gamma filter exists in the DOM
    ensureGammaFilter(gamma);

    cCtx.save();
    cCtx.globalAlpha = alpha;
    cCtx.globalCompositeOperation = blendMode;
    const gammaFilter = gamma !== 1 ? ` url(#feedbackGamma)` : '';
    cCtx.filter = `brightness(${brightness}) contrast(${contrast}) saturate(${saturate})${gammaFilter}`;

    // Apply zoom + rotation transform (same for all modes)
    // Beat rotation adds discrete steps on top of continuous rotation
    const totalAngle = angle + beatRotationIncrement;
    cCtx.translate(w / 2, h / 2);
    cCtx.rotate(totalAngle);
    cCtx.scale(zoom, zoom);
    cCtx.translate(-w / 2, -h / 2);

    if (feedbackWrapMode === 'none') {
        cCtx.drawImage(img, 0, 0);
    } else if (feedbackWrapMode === 'repeat') {
        // Simple repeat: draw the same image tiled without mirroring
        const zoomFactor = Math.max(1, 1 / Math.max(zoom, 0.01));
        const rotationFactor = Math.abs(Math.cos(angle)) + Math.abs(Math.sin(angle));
        const range = Math.ceil(Math.max(zoomFactor, rotationFactor) * 0.7) + 1;
        for (let col = -range; col <= range; col++) {
            for (let row = -range; row <= range; row++) {
                cCtx.save();
                cCtx.translate(col * w, row * h);
                cCtx.drawImage(img, 0, 0);
                cCtx.restore();
            }
        }
    } else {
        // Dynamic grid size: more tiles needed when zooming out (zoom < 1) or rotating
        const zoomFactor = Math.max(1, 1 / Math.max(zoom, 0.01));
        const rotationFactor = Math.abs(Math.cos(angle)) + Math.abs(Math.sin(angle));
        const range = Math.ceil(Math.max(zoomFactor, rotationFactor) * 0.7) + 1;
        
        for (let col = -range; col <= range; col++) {
            for (let row = -range; row <= range; row++) {
                const flipX = (feedbackWrapMode !== 'tile') && Math.abs(col) % 2 === 1;
                const flipY = (feedbackWrapMode !== 'tile') && Math.abs(row) % 2 === 1;

                cCtx.save();
                if (flipX || flipY) {
                    cCtx.translate(col * w + (flipX ? w : 0), row * h + (flipY ? h : 0));
                    cCtx.scale(flipX ? -1 : 1, flipY ? -1 : 1);
                } else {
                    cCtx.translate(col * w, row * h);
                }
                cCtx.drawImage(img, 0, 0);
                cCtx.restore();
            }
        }
    }

    cCtx.filter = 'none';
    cCtx.restore();
}

/**
 * drawWithWrap
 *
 * Applies geometryWrapMode to a drawing function (for geometries, not just images).
 * Dynamic grid ensures edges are filled even with rotation/zoom.
 *
 * Usage:
 *   drawWithWrap(cCtx, w, h, geometryWrapMode, (ctx) => {
 *     ctx.strokeStyle = 'red';
 *     ctx.beginPath();
 *     ctx.arc(w/2, h/2, 50, 0, Math.PI*2);
 *     ctx.stroke();
 *   });
 */
export function drawWithWrap(cCtx, w, h, geometryWrapMode, drawFn) {
    if (geometryWrapMode === 'none') {
        drawFn(cCtx);
    } else if (geometryWrapMode === 'repeat') {
        const range = 1;
        for (let col = -range; col <= range; col++) {
            for (let row = -range; row <= range; row++) {
                cCtx.save();
                cCtx.translate(col * w, row * h);
                drawFn(cCtx);
                cCtx.restore();
            }
        }
    } else {
        const range = 1;
        for (let col = -range; col <= range; col++) {
            for (let row = -range; row <= range; row++) {
                const flipX = (geometryWrapMode !== 'tile') && Math.abs(col) % 2 === 1;
                const flipY = (geometryWrapMode !== 'tile') && Math.abs(row) % 2 === 1;

                cCtx.save();
                if (flipX || flipY) {
                    cCtx.translate(col * w + (flipX ? w : 0), row * h + (flipY ? h : 0));
                    cCtx.scale(flipX ? -1 : 1, flipY ? -1 : 1);
                } else {
                    cCtx.translate(col * w, row * h);
                }
                drawFn(cCtx);
                cCtx.restore();
            }
        }
    }
}

/**
 * Applies circular motion transformation to context.
 */
export function applyCircularMotion(ctx, w, h, diameter, depth, timeOffset, speed = 1) {
    if (diameter > 0 && depth > 0) {
        const radius = diameter / 2;
        const angle = timeOffset * speed;
        const offsetX = Math.cos(angle) * radius;
        const offsetY = Math.sin(angle) * radius;
        const zDepth = Math.sin(angle * 0.5);
        const scale = 1 + zDepth * depth;

        ctx.translate(w / 2, h / 2);
        ctx.scale(scale, scale);
        ctx.translate(offsetX, offsetY);
        ctx.translate(-w / 2, -h / 2);
    }
}