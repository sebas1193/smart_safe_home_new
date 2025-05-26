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

// Ruta de autenticación
// Ruta de autenticación
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

app.get('/status_logs_owner', async (req, res) => {
    try {
        const { user_id, owner_id } = req.query;
        const response = await axios.get(`http://localhost:1880/status_logs_owner?user_id=${user_id}&owner_id=${owner_id}`);
        res.json(response.data); // devuelves los logs como JSON
    } catch (err) {
        res.status(500).json({ error: 'Error fetching logs' });
    }
});

// Vistas según rol
app.get('/admin', (req, res) => { 
    if (req.session.loggedin && req.session.role === 'admin') { 
        return res.render('admin', { 
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
