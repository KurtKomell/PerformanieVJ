import { useEffect, useRef } from 'react';
import { drawFeedback, drawWithWrap, applyCircularMotion } from '@/utils/feedbackWrap';
import { createBeatDetector } from '@/utils/beatDetection';

import { mergeFreqMapping } from '@/utils/freqMerge';

export default function CircularPreset({ audioDataRef, canvasRef, feedbackSettings, freqMappingRef }) {
    const animationRef = useRef(null);
    const settingsRef = useRef(feedbackSettings);
    const freqMapRef = useRef(freqMappingRef);
    const bufA = useRef(null);
    const bufB = useRef(null);
    const frameRef = useRef(0);
    const angleAccum = useRef(0);
    const beatRotation = useRef(0);
    const smoothBass = useRef(0);
    const smoothMid = useRef(0);
    const beatDetector = useRef(createBeatDetector(0.5, 80));

    useEffect(() => { settingsRef.current = feedbackSettings; }, [feedbackSettings]);
    useEffect(() => { freqMapRef.current = freqMappingRef; }, [freqMappingRef]);


    useEffect(() => {
        const canvas = canvasRef.current;
        if (!canvas) return;
        const ctx = canvas.getContext('2d');

        const ensureBuffers = (w, h) => {
            if (!bufA.current || bufA.current.width !== w || bufA.current.height !== h) {
                const a = document.createElement('canvas');
                a.width = w; a.height = h;
                a.getContext('2d').fillRect(0, 0, w, h);
                bufA.current = a;
                const b = document.createElement('canvas');
                b.width = w; b.height = h;
                b.getContext('2d').fillRect(0, 0, w, h);
                bufB.current = b;
            }
        };

        const render = () => {
            const w = canvas.width;
            const h = canvas.height;
            ensureBuffers(w, h);

            const { frequencies, bass, mid, average } = audioDataRef.current;
            const s = mergeFreqMapping(settingsRef.current || {}, freqMapRef.current);
            frameRef.current++;

            smoothBass.current += (bass - smoothBass.current) * 0.12;
            smoothMid.current += (mid - smoothMid.current) * 0.12;

            // Beat detection for feedback rotation
            if (beatDetector.current.detect(bass)) {
                beatRotation.current += Math.PI / 24; // 7.5° per kick
            }

            // Ping-Pong Buffer System
            const pp = {
                read: frameRef.current % 2 === 0 ? bufA.current : bufB.current,
                write: frameRef.current % 2 === 0 ? bufB.current : bufA.current,
            };
            const cCtx = pp.write.getContext('2d');

            const rotMult = s.rotationSpeed ?? 1.0;
            const zoomMult = s.zoomSpeed ?? 0;
            const zoomBase = s.zoomBase ?? 0;
            const bassThreshold = 0.35;
            if (bass > bassThreshold) {
                angleAccum.current += bass * rotMult * 0.05;
            }
            const dynamicZoom = 1.0 + zoomBase * 0.01 + smoothMid.current * zoomMult * 0.15;

            cCtx.fillStyle = 'rgba(10, 10, 15, 0.2)';
            cCtx.fillRect(0, 0, w, h);

            drawWithWrap(cCtx, w, h, s.geometryWrapMode ?? 'none', (drawCtx) => {
                drawCtx.save();
                drawCtx.globalAlpha = s.cameraBlend ?? 1;
                drawCtx.translate(w / 2, h / 2);
                drawCtx.rotate(angleAccum.current);
                drawCtx.translate(-w / 2, -h / 2);
                applyCircularMotion(drawCtx, w, h, s.circularMotionDiameter ?? 0, s.circularMotionDepth ?? 0, frameRef.current * 0.02, s.circularMotionSpeed ?? 1);

                const cx = w / 2;
                const cy = h / 2;
                const baseRadius = Math.min(w, h) * 0.25;
                const numBars = frequencies.length;

                for (let i = 0; i < numBars; i++) {
                    const angle = (i / numBars) * Math.PI * 2 - Math.PI / 2;
                    const value = frequencies[i] / 255;
                    const barLen = value * baseRadius * 0.8;
                    const hue = (i / numBars) * 360 + bass * 60;

                    const x1 = cx + Math.cos(angle) * baseRadius;
                    const y1 = cy + Math.sin(angle) * baseRadius;
                    const x2 = cx + Math.cos(angle) * (baseRadius + barLen);
                    const y2 = cy + Math.sin(angle) * (baseRadius + barLen);

                    drawCtx.strokeStyle = `hsla(${hue}, 100%, 60%, ${0.5 + value * 0.5})`;
                    drawCtx.lineWidth = 2;
                    if (value > 0.7) {
                        drawCtx.shadowBlur = 10;
                        drawCtx.shadowColor = `hsla(${hue}, 100%, 60%, 0.8)`;
                    }
                    drawCtx.beginPath();
                    drawCtx.moveTo(x1, y1);
                    drawCtx.lineTo(x2, y2);
                    drawCtx.stroke();
                    drawCtx.shadowBlur = 0;
                }

                drawCtx.beginPath();
                drawCtx.arc(cx, cy, baseRadius * (0.8 + smoothBass.current * 0.3), 0, Math.PI * 2);
                drawCtx.strokeStyle = `hsla(280, 100%, 70%, ${0.3 + smoothBass.current * 0.5})`;
                drawCtx.lineWidth = 2;
                drawCtx.stroke();

                drawCtx.restore();
            });

            drawFeedback(cCtx, pp.read, w, h, s.feedbackWrapMode ?? 'none', dynamicZoom, beatRotation.current, s);

            ctx.filter = `brightness(${s.brightness ?? 1}) contrast(${s.contrast ?? 1}) saturate(${s.saturation ?? 1})`;
            ctx.drawImage(pp.write, 0, 0);
            ctx.filter = 'none';

            animationRef.current = requestAnimationFrame(render);
        };

        render();
        return () => { if (animationRef.current) cancelAnimationFrame(animationRef.current); };
    }, [canvasRef]);

    return null;
}