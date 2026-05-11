var API='';
function get(u){return fetch(API+u).then(r=>r.json())}
function post(u,d){return fetch(API+u,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)}).then(r=>r.json())}
function toast(m){var e=document.createElement('div');e.className='toast';e.textContent=m;document.body.appendChild(e);setTimeout(function(){e.classList.add('show')},10);setTimeout(function(){e.classList.remove('show');setTimeout(function(){e.remove()},300)},2000)}
