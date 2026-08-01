import { BrowserRouter, MemoryRouter, Route, Router } from "react-router"
import { CSRFContext, CSRFProvider } from "../providers/CSRFProvider"
import Login from "./pages/Login"
import SignUp from "./pages/SignUp"
import { useCookies } from "react-cookie"
import { Navigate, Outlet } from 'react-router-dom';
import DirectoryManager from "./pages/DirectoryManager"

const ProtectedRoute = ({ isAuthenticated }) => {
    if (!isAuthenticated) {
        return <Navigate to="/login" replace />;
    }
    return <Outlet />;
};

export default function App() {

    let [cookies, setCookie, removeCookie] = useCookies()


    return <MemoryRouter>
        <CSRFProvider>
            <Route element={<Login />} path="/login" />
            <Route element={<SignUp />} path="/signup" />
            <Route index element={<ProtectedRoute isAuthenticated={cookies.username} />} path="/main">
                <DirectoryManager username={cookies.username} />
            </Route>
        </CSRFProvider>
    </MemoryRouter>

}
