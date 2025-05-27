import express from 'express';
import session from 'express-session';
import bodyParser from 'body-parser';
import axios from 'axios';
import { dirname, join } from 'path';
import { fileURLToPath } from 'url';

const app = express();
const port = 3000;
const __dirname = dirname(fileURLToPath(import.meta.url));

// Middlewares
app.use(bodyParser.urlencoded({ extended: false }));
app.use(bodyParser.json());
app.use(session({
    secret: 'smart_safe_home_secret',
    resave: false,
    saveUninitialized: true
}));

// Vistas
app.set('views', join(__dirname, 'views'));
app.set('view engine', 'ejs');

// Rutas públicas
app.get('/', (req, res) => res.render('index'));
app.get('/login', (req, res) => res.render('login'));

function isAuthenticated(req, res, next) {
  if (req.session.loggedin) {
    return next();
  } else {
    return res.redirect('/login');
  }
}

// Ruta de autenticación
app.post('/auth', async (req, res) => { 
    const { user, passwd } = req.body; 
 
    try { 
        // Volver a GET con query parameters como era originalmente
        const response = await axios.post(`http://localhost:1880/login?username=${encodeURIComponent(user)}&password=${encodeURIComponent(passwd)}`);
        const result = response.data; 
 
        if (result.success) { 
            // Guardar todos los datos del usuario en la sesión
            req.session.loggedin = true; 
            req.session.userData = result.data; // Guardamos todos los datos
            req.session.user = result.data.username;
            req.session.userId = result.data.id;
            req.session.role = result.data.role;
            req.session.firstName = result.data.first_name;
            req.session.lastName = result.data.last_name;
 
            if (result.data.role === 'admin') { 
                return res.redirect('/admin'); 
            } else if (result.data.role === 'emergency_contact') { 
                return res.redirect('/user'); 
            } else { 
                return res.send('Rol no reconocido'); 
            } 
        } else { 
            res.send('<script>alert("Usuario o contraseña incorrectos"); window.location.href="/login";</script>'); 
        } 
    } catch (error) { 
        console.error('Error completo:', error);
        if (error.response) {
            console.error('Datos de respuesta:', error.response.data);
            console.error('Estado:', error.response.status);
        }
        res.send("Error al autenticar al usuario"); 
    } 
});

app.get('/sign_up', (req, res) => res.render('sign_up'));

app.get('/sign_up_owner', isAuthenticated, (req, res) => {
  res.render('sign_up_owner', {
    user: req.session.user,
    userId: req.session.userId,
    firstName: req.session.firstName,
    lastName: req.session.lastName,
    role: req.session.role
  });
});

app.get('/status_logs_owner', async (req, res) => {
    try {
        const { user_id, owner_id } = req.query;
        const response = await axios.get(`http://localhost:1880/status_logs_owner?user_id=${user_id}&owner_id=${owner_id}`);
        res.json(response.data); // devuelves los logs como JSON
    } catch (err) {
        res.status(500).json({ error: 'Error fetching logs' });
    }
});

// Agregar esta ruta en tu index.js después de las otras rutas
// trae datos del owner.
app.get('/owner_info_contact', isAuthenticated, async (req, res) => {
  try {
    const { id_owner } = req.query;
    
    // Validar que se proporcione el id_owner
    if (!id_owner) {
      return res.redirect('/user?error=missing_owner_id');
    }

    // Llamar al endpoint de Node-RED para obtener la información del owner
    const response = await axios.get(`http://localhost:1880/owner_info?username=${encodeURIComponent(req.session.user)}&id_owner=${id_owner}`);
    const result = response.data;

    if (result.success) {
      // Renderizar la vista con la información del owner y datos de sesión
      res.render('owner_info_contact', {
        user: req.session.user,
        userId: req.session.userId,
        firstName: req.session.firstName,
        lastName: req.session.lastName,
        role: req.session.role,
        userData: req.session.userData,
        ownerInfo: result.data.house_owner,
        requestedBy: result.data.requested_by
      });
    } else {
      // Si hay error, redirigir a user con mensaje de error
      res.redirect(`/user?error=${encodeURIComponent(result.message)}`);
    }
  } catch (error) {
    console.error('Error fetching owner info:', error);
    res.redirect('/user?error=server_error');
  }
});
// Vistas según rol
// Reemplaza tu ruta /admin actual con esta versión mejorada

// Reemplaza tu ruta /admin actual con esta versión

app.get('/admin', async (req, res) => { 
    if (req.session.loggedin && req.session.role === 'admin') { 
        try {
            // Llamar a todos los endpoints en paralelo
            const [logsResponse, countResponse, activeNodesResponse, inactiveNodesResponse] = await Promise.all([
                axios.get(`http://localhost:1880/get_all_info_nodes_users?username=${encodeURIComponent(req.session.user)}`),
                axios.get(`http://localhost:1880/count_house_owners?username=${encodeURIComponent(req.session.user)}`),
                axios.get(`http://localhost:1880/count_active_nodes?username=${encodeURIComponent(req.session.user)}`),
                axios.get(`http://localhost:1880/count_inactive_nodes?username=${encodeURIComponent(req.session.user)}`)
            ]);

            const logsResult = logsResponse.data;
            const countResult = countResponse.data;
            const activeNodesResult = activeNodesResponse.data;
            const inactiveNodesResult = inactiveNodesResponse.data;

            // Renderizar vista con todos los datos
            return res.render('admin', { 
                user: req.session.user,
                userId: req.session.userId,
                firstName: req.session.firstName,
                lastName: req.session.lastName,
                role: req.session.role,
                userData: req.session.userData,
                logs: logsResult.success ? logsResult.data : [],
                totalOwners: countResult.success ? countResult.data.total_house_owners : 0,
                ownersMessage: countResult.success ? countResult.data.message : "0 elderly monitored",
                activeNodes: activeNodesResult.success ? activeNodesResult.data.active_nodes : 0,
                inactiveNodes: inactiveNodesResult.success ? inactiveNodesResult.data.inactive_nodes : 0
            });
        } catch (error) {
            console.error('Error fetching admin data:', error);
            // En caso de error, renderizar con datos vacíos
            return res.render('admin', { 
                user: req.session.user,
                userId: req.session.userId,
                firstName: req.session.firstName,
                lastName: req.session.lastName,
                role: req.session.role,
                userData: req.session.userData,
                logs: [],
                totalOwners: 0,
                ownersMessage: "Error loading data",
                activeNodes: 0,
                inactiveNodes: 0
            });
        }
    } 
    res.redirect('/login'); 
});

// Agregar esta ruta en tu index.js

app.get('/create_admin', isAuthenticated, (req, res) => {
  // Solo admins pueden acceder a crear otros admins
  if (req.session.role !== 'admin') {
    return res.redirect('/login');
  }

  res.render('create_admin', {
    user: req.session.user,
    userId: req.session.userId,
    firstName: req.session.firstName,
    lastName: req.session.lastName,
    role: req.session.role,
    userData: req.session.userData
  });
});

app.get('/user', (req, res) => { 
    if (req.session.loggedin && req.session.role === 'emergency_contact') { 
        return res.render('user', { 
            user: req.session.user,
            userId: req.session.userId,
            firstName: req.session.firstName,
            lastName: req.session.lastName,
            role: req.session.role,
            userData: req.session.userData // Todos los datos en un objeto
        }); 
    } 
    res.redirect('/login'); 
});

app.get('/log_out', (req, res) => {
    // Destruir la sesión actual
    req.session.destroy((err) => {
        if (err) {
            console.error('Error destroying session:', err);
            res.status(500).send('Error destroying session');
        } else {
            // Redirigir al usuario al inicio después del logout
            res.redirect('/');
        }
    });
});
// Arrancar servidor
app.listen(port, () => {
    console.log(`Servidor corriendo en http://localhost:${port}`);
});
