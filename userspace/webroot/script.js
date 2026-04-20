(function () {
  'use strict';

  /* MountZero v4 WebUI - SUSFS-Free */
  
  /* Binary locations - check in order */
  const BIN_PATHS = [
    '/data/adb/ksu/bin/mz4',
    '/data/adb/modules/mountzero_v4/bin/mz4',
    '/data/adb/modules/mountzero_v4/system/bin/mzctl',
    '/system/bin/mz4'
  ];
  
  const CONFIG = '/data/adb/mountzero_v4/config.toml';
  
  /* KernelSU exec bridge */
  async function runCmd(cmd) {
    try {
      if (typeof exec === 'function') {
        let r = await exec(cmd, {});
        return (r.stdout || '') + (r.stderr || '');
      } else if (typeof ksu !== 'undefined' && ksu.exec) {
        return new Promise((resolve) => {
          let cb = 'mz' + Date.now();
          window[cb] = (e, o) => { resolve((o||'')); delete window[cb]; };
          ksu.exec(cmd, '{}', cb);
        });
      }
    } catch (e) { return 'Error: ' + e.message; }
    return 'Bridge unavailable';
  }

  /* Find working mz4 binary */
  async function findMZ() {
    for (let p of BIN_PATHS) {
      let r = await runCmd('test -x ' + p + ' && echo YES || echo NO');
      if (r.trim() === 'YES') return p;
    }
    return BIN_PATHS[0];
  }

  /* Run mz4 */
  async function runMZ(args) {
    let bin = await findMZ();
    return runCmd(bin + ' ' + args + ' 2>&1');
  }

  /* Tab functions - all using mz4 only */
  async function loadStatus() {
    let ver = await runMZ('version');
    let stat = await runMZ('status');
    document.getElementById('status-content').innerHTML = ver + '<br>Engine: ' + (stat === '1' ? 'Enabled' : 'Disabled');
  }

  async function loadModules() {
    let r = await runMZ('list-paths 2>&1');
    document.getElementById('modules-content').innerHTML = r || 'No paths hidden';
  }

  async function loadRules() {
    let r = await runMZ('get-hidden 2>&1');
    document.getElementById('rules-content').innerHTML = r || 'No rules';
  }

  async function loadConfig() {
    let r = await runCmd('cat ' + CONFIG + ' 2>/dev/null');
    document.getElementById('config-content').innerHTML = r || 'Not found';
  }

  async function loadGuard() {
    let r = await runMZ('get-uname 2>&1');
    document.getElementById('guard-content').innerHTML = r || 'Not configured';
  }

  async function loadHidingConfig() {
    let m = await runMZ('get-mounts 2>&1');
    let mp = await runMZ('get-maps 2>&1');
    document.getElementById('hiding-content').innerHTML = 'Mounts: ' + (m||'None') + '<br>Maps: ' + (mp||'None');
  }

  async function loadTools() {
    let r = await runMZ('version');
    document.getElementById('tools-content').innerHTML = r;
  }

  /* Tab click handlers */
  document.querySelectorAll('.tab').forEach(t => {
    t.addEventListener('click', function() {
      document.querySelectorAll('.tab').forEach(x => x.classList.remove('active'));
      document.querySelectorAll('.tab-content').forEach(x => x.classList.remove('active'));
      this.classList.add('active');
      document.getElementById('tab-' + this.dataset.tab).classList.add('active');
      
      switch(this.dataset.tab) {
        case 'status': loadStatus(); break;
        case 'modules': loadModules(); break;
        case 'rules': loadRules(); break;
        case 'config': loadConfig(); break;
        case 'guard': loadGuard(); break;
        case 'hiding': loadHidingConfig(); break;
        case 'tools': loadTools(); break;
      }
    });
  });

  loadStatus();
})();