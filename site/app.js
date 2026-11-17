(function () {
  'use strict';
  var root = document.documentElement;
  var KEY = 'musica-lang';
  var T = {
    en: { copy: 'Copy', copied: 'Copied', select: 'Selected: press Ctrl+C' },
    fr: { copy: 'Copier', copied: 'Copié', select: 'Sélectionné : Ctrl+C' }
  };
  var ATTRS = ['alt', 'title', 'aria-label'];
  var lang = 'en';

  function read() { try { return localStorage.getItem(KEY); } catch (e) { return null; } }
  function write(v) { try { localStorage.setItem(KEY, v); } catch (e) {} }

  function labels() {
    [].forEach.call(document.querySelectorAll('.copy .lbl'), function (s) { s.textContent = T[lang].copy; });
  }

  function setLang(l) {
    lang = l;
    root.lang = l;
    root.setAttribute('data-lang', l);
    ATTRS.forEach(function (a) {
      [].forEach.call(document.querySelectorAll('[data-fr-' + a + ']'), function (el) {
        if (!el.hasAttribute('data-en-' + a)) el.setAttribute('data-en-' + a, el.getAttribute(a));
        el.setAttribute(a, el.getAttribute('data-' + l + '-' + a));
      });
    });
    [].forEach.call(document.querySelectorAll('[data-set-lang]'), function (b) {
      b.setAttribute('aria-pressed', b.getAttribute('data-set-lang') === l ? 'true' : 'false');
    });
    labels();
  }

  var saved = read();
  setLang(saved === 'fr' || saved === 'en' ? saved : (/^fr/i.test(navigator.language || '') ? 'fr' : 'en'));

  [].forEach.call(document.querySelectorAll('[data-set-lang]'), function (b) {
    b.addEventListener('click', function () {
      var l = b.getAttribute('data-set-lang');
      setLang(l);
      write(l);
    });
  });

  /* Copy buttons */
  var status = document.getElementById('status');
  function flash(btn, msg) {
    var s = btn.querySelector('.lbl');
    s.textContent = msg;
    status.textContent = msg;
    setTimeout(function () { s.textContent = T[lang].copy; status.textContent = ''; }, 2000);
  }
  function fallback(el, btn) {
    var sel = window.getSelection(), r = document.createRange();
    r.selectNodeContents(el);
    sel.removeAllRanges();
    sel.addRange(r);
    var ok = false;
    try { ok = document.execCommand('copy'); } catch (e) {}
    flash(btn, ok ? T[lang].copied : T[lang].select);
  }
  [].forEach.call(document.querySelectorAll('.copy'), function (btn) {
    btn.hidden = false;
    btn.addEventListener('click', function () {
      var el = document.getElementById(btn.getAttribute('data-target'));
      var text = el.textContent.trim();
      if (navigator.clipboard && window.isSecureContext) {
        navigator.clipboard.writeText(text).then(function () { flash(btn, T[lang].copied); },
                                                 function () { fallback(el, btn); });
      } else { fallback(el, btn); }
    });
  });
  labels();

  /* Tabs */
  var tabs = document.querySelector('.tabs');
  if (tabs) {
    var list = tabs.querySelector('[role=tablist]');
    var items = [].slice.call(list.querySelectorAll('[role=tab]'));
    var select = function (t, focus) {
      items.forEach(function (x) {
        var on = x === t;
        x.setAttribute('aria-selected', on ? 'true' : 'false');
        x.tabIndex = on ? 0 : -1;
        document.getElementById(x.getAttribute('aria-controls')).hidden = !on;
      });
      if (focus) t.focus();
    };
    list.hidden = false;
    tabs.classList.add('tabs-on');
    items.forEach(function (t, i) {
      t.addEventListener('click', function () { select(t); });
      t.addEventListener('keydown', function (e) {
        var n = null, len = items.length;
        if (e.key === 'ArrowRight') n = items[(i + 1) % len];
        else if (e.key === 'ArrowLeft') n = items[(i - 1 + len) % len];
        else if (e.key === 'Home') n = items[0];
        else if (e.key === 'End') n = items[len - 1];
        if (n) { e.preventDefault(); select(n, true); }
      });
    });
    select(items[0]);
  }

  /* Reduced motion: no autoplay, show the poster */
  var v = document.getElementById('demo-video');
  if (v && window.matchMedia && matchMedia('(prefers-reduced-motion: reduce)').matches) {
    v.removeAttribute('autoplay');
    v.controls = true;
    v.load();
  }
})();
